#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <time.h>
#include <ctype.h>
#include <termios.h>


//#define PALL //print all
#define BUFFSIZE 1024
#define MSGLIM 100

int main( int argc, const char* argv[] )
{
    struct in_addr inaddr;
    int port = 12345; //default

    //adresa
    if(argc >=2) //bo posledny je null
    {
        #ifdef PALL
        printf("arg 1 :%s \n", argv[1]);
        #endif

        if( inet_pton(AF_INET, argv[1], &inaddr)  != 1)
        {
            fprintf(stderr,"Adresu nejde pouzit\n");
            return 1;
        }
    }
    else //kedze povinny argument tu sa riesi ak neni zadany
    {
        fprintf(stderr,"Adresa nebola zadana\n");
        return 1;
    }
    //port
    if(argc >=3)
    {
        #ifdef PALL
        printf("arg 2 :%s \n", argv[2]);
        #endif // PALL

        port = atoi(argv[2]);

        if(port <= 0 || port > 65535)
        {
            fprintf(stderr,"Neplatny port\n");
            return 1;
        }
    }

    int sour_socket;

    sour_socket = socket(PF_INET, SOCK_DGRAM, 0);
    if(sour_socket < 0)
    {
        fprintf(stderr,"Nejde vytvorit socket\n");
        return 1;
    }

    //spravime sock_addr
    struct sockaddr_in dest_sock_addr;       //kam
    memset((char *) &dest_sock_addr, 0, sizeof(dest_sock_addr));
    dest_sock_addr.sin_family = AF_INET;
    dest_sock_addr.sin_port = htons(port);
    dest_sock_addr.sin_addr = inaddr;

    struct sockaddr_in sour_sock_addr;       //odkial
    memset((char *) &sour_sock_addr, 0, sizeof(sour_sock_addr));
    sour_sock_addr.sin_family = AF_INET;
    sour_sock_addr.sin_port = htons(port);
    sour_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);


    //bindneme na lokalnu adresu na source socket
    if(bind(sour_socket, (struct sockaddr *) &sour_sock_addr, sizeof(sour_sock_addr)) != 0)
    {
        fprintf(stderr,"Nejde bindnut source socket\n");
        return 1;
    }

    //pozicane z prednasky
    struct termios termattr;
    if (tcgetattr(0, &termattr) < 0)
    {
        perror("tcgetattr");
        exit(1);
    }
    termattr.c_lflag &= ~ICANON;
    if (tcsetattr(0, TCSANOW, &termattr) < 0)
    {
        perror("tcsetattr");
        exit(1);
    }


    char buf[BUFFSIZE] = "";
    int gotlength = 0;
    fd_set fds;
    bool print = true;

    char sendb[BUFFSIZE] = "";
    memset(sendb,0,BUFFSIZE);
    sendb[0] = 'i';sendb[1] = 'n';sendb[2] = ':';
    int sendb_poz = 3;

    char msgbuffer[BUFFSIZE * MSGLIM] = "";
    int msgb_poz = 0; /* bude obsahovat cislo prveho volneho miesta, cize
                         pocet charov v msgbuffer kere sa maju vyprintit*/

    bool f = false; /*aby nam no nevynechavalo prvy znak po entru*/
    //main loop
    while(1)
    {

        FD_ZERO(&fds);
        //pridame STDIN
        FD_SET(STDIN_FILENO, &fds);
        //pridame socet
        FD_SET(sour_socket,&fds);

        int smth = select( sour_socket + 1 , &fds , NULL , NULL , NULL);

        if(smth <0)
        {
            fprintf(stderr, "selector error");
            return 1;
        }

        if(FD_ISSET(sour_socket, &fds))//prisla sprava
        {
            #ifdef PALL
            printf("(Prsial sprava)");
            #endif


            /*MSG_DONTWAIT tu nepotrebujeme bo sem sa ide iba ak sme neco
            dostali takze neni na co cakat, a teda vzdy mame aktualnu dlzku(gotlengt)*/
            memset(buf,0,BUFFSIZE);
            gotlength = recvfrom(sour_socket,buf, BUFFSIZE,0,NULL, NULL);
            if(gotlength > 0)
            {

                if(print == true)
                {
                    printf("%.*s",gotlength,buf);//vypise gotlength charov z buf
                }
                else
                {
                    int a;
                    for(a = 0;a < gotlength && msgb_poz + a < MSGLIM*BUFFSIZE;a++)
                    {
                        msgbuffer[msgb_poz+a] = buf[a];
                    }
                    msgb_poz +=a+1; //povie kde je dalsi volni, resp pocet charov v msgbuffer
                }
            }
            if(errno != 0)
            {
                perror(strerror(errno));
                errno = 0;
            }
        }

        if(FD_ISSET(0, &fds))//vstup
        {
            if(f == true)/*aby nam no nevynechavalo prvy znak po entru*/
            {
                f = false;
                continue;
            }
            char c = 0;
            c = getc(stdin);
            #ifdef PALL
            printf("( %c : %d )\n", c, (int)c);
            #endif

            if((int)c == 10)//enter
            {
                gotlength = strlen(sendb);
                sendb[gotlength] = '\n';
                gotlength++;
                sendto(sour_socket,sendb,gotlength, 0,(struct sockaddr *)&dest_sock_addr, sizeof(dest_sock_addr));

                if(errno != 0)
                {
                    perror(strerror(errno));
                    errno = 0;
                }

                print = true;
                f = true;
                memset(sendb,0,BUFFSIZE);
                sendb[0] = 'i';sendb[1] = 'n';sendb[2] = ':';
                sendb_poz = 3;

                #ifdef PALL
                printf("<-KONEC IN ");
                #endif
            }
            else if(sendb_poz< BUFFSIZE-1) //na konci jedno miseto pre '\n'
            {
                sendb[sendb_poz] = c;
                sendb_poz++;
                print = false;
            }
        }

        if(print == true && msgb_poz >0)
        {
            fwrite ( msgbuffer, sizeof(char), msgb_poz, stdout);
            //printf("%.*s", ,msgbuffer); ma problem ze vypisuje iba po '\n'
            msgb_poz = 0;
            memset(msgbuffer,0, BUFFSIZE*MSGLIM);
        }

    }


    return 0;
}
