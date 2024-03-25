#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <future>
#include <csignal>

using namespace std;

#define PORT 2728

/* portul de conectare la server*/
int port, fd_curent, sd;
bool disconnect = false;
char answer[255];
bool in_the_game = false;

void signalHandler(int signum)
{
  disconnect = true;
  cout << endl
       << "Ctrl+C pressed! "<<endl;
  fflush(stdout);
  /* pregatim mesajul de deconectare */
  char msg[11];
  strcpy(msg, "disconnect");
  msg[strlen(msg)] = '\0';

  /* trimitem mesajul de deconectare */
  if (write(sd, msg, strlen(msg)) <= 0)
  {
    perror("Error at write!");
    return;
  }
  if (in_the_game == false)
  {
    cout << "[Client] Press enter to quit the game!" << endl;
    fflush(stdout);
  }

  /* inchidem conexiunea */
  close(sd);
  return;
}

void handleAnswers() /* functie care se ocupa de citirea raspunsurilor in max 15 sec*/
{
  fd_set readfds;
  struct timeval timeout;
  char buffer[255];
  bzero(buffer, 255);
  bzero(answer, 255);
  timeout.tv_sec = 15;
  timeout.tv_usec = 0;

  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);
  int result;
  if (!disconnect)
  {
    result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
  }
  else
  {
    return;
  }

  if (result == -1)
  {
    // perror("[Client] Error in select");
    return;
  }
  else if (result == 0)
  {
    printf("Timeout! No answer given.\n");
    strcpy(answer, " ");
  }
  else
  {
    ssize_t bytesRead;
    if (!disconnect)
    {
      bytesRead = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    }
    else
    {
      return;
    }

    if (bytesRead > 0)
    {
      buffer[bytesRead] = '\0';
      strcpy(answer, buffer);
    }
    else if (bytesRead == 0)
    {
      printf("[Client] End of file reached.\n");
    }
    else
    {
      perror("[Client] Error reading from stdin");
    }
  }
}

int main()
{
  struct sockaddr_in server; // structura folosita pentru conectare
  char msg[500];             // mesajul trimis

  /* creare socket */
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("Eroare la socket().\n");
    return errno;
  }

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = inet_addr("0");
  /* portul de conectare */
  server.sin_port = htons(PORT);

  /* ne conectam la server */
  if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
  {
    perror("[client]Eroare la connect().\n");
    return errno;
  }

  signal(SIGINT, signalHandler);

  bzero(msg, 500);
  /* citirea welcomeMessage dat de server */
  if (!disconnect)
  {
    if (read(sd, msg, sizeof(msg)) < 0)
    {
      perror("[client]Eroare la read() de la server.\n");
      return errno;
    }
  }
  else
  {
    return 0;
  }
  /* afisam mesajul primit */
  printf("%s\n", msg);

  do
  {
    /* citirea comanda login/signup*/
    bzero(msg, 500);
    printf("Introduceti comanda: ");
    fflush(stdout);
    if (!disconnect)
    {
      read(0, msg, 500);
    }
    else
    {
      return 0;
    }
    msg[strlen(msg)] = '\0';

    /* trimiterea comenzii la server */
    if (!disconnect)
    {
      if (write(sd, msg, strlen(msg)) <= 0)
      {
        perror("[client]Eroare la write() spre server.\n");
        return errno;
      }
    }
    else
    {
      return 0;
    }

    /* citirea raspunsului dat de server: login/signup successful? */
    bzero(msg, 500);
    if (!disconnect)
    {
      if (read(sd, msg, sizeof(msg)) < 0)
      {
        perror("[client]Eroare la read() de la server.\n");
        return errno;
      }
    }
    else
    {
      return 0;
    }

    /* afisam mesajul primit */
    printf("%s\n", msg);
    fflush(stdout);
  } while (strstr(msg, "fara") || strstr(msg, "deja"));

  /* dupa logarea cu succes, serverul asteapta raspuns: */
  /* clientul doreste o camera noua? [y/n] */
  printf("Introduceti raspuns: ");
  fflush(stdout);
  bzero(msg, 500);
  if (!disconnect)
  {
    read(0, msg, 500);
  }
  else
  {
    return 0;
  }
  msg[strlen(msg)] = '\0';

  /* trimiterea raspuns la server: y/n*/
  if (!disconnect)
  {
    if (write(sd, msg, strlen(msg)) <= 0)
    {
      perror("[client]Eroare la write() spre server.\n");
      return errno;
    }
  }
  else
  {
    return 0;
  }

  /* citim status cerere: room created?/enter id? */
  bzero(msg, 500);
  if (!disconnect)
  {
    if (read(sd, msg, sizeof(msg)) < 0)
    {
      perror("[client]Eroare la read() de la server.\n");
      return errno;
    }
  }
  /* afisam mesajul primit */
  printf("%s\n", msg);

  /* cazuri de raspuns ale serverului */
  if (strstr(msg, "Ne pare rau") != NULL)
  {
    /* am vrut sa deschidem o camera noua
        dar avem o eroare de la server */
    printf("Eroare la creare camera. Reveniti mai tarziu!\n");
    close(sd);
  }
  else if (strstr(msg, "Camera s-a creat!") != NULL)
  {
    /* gata, camera exista,
        urmeaza sa asteptam colegi de joc*/
    bzero(msg, 500);
    printf("Start?\n");
    fflush(stdout);
    if (!disconnect)
    {
      read(0, msg, 500);
    }
    else
    {
      return 0;
    }
    msg[strlen(msg)] = '\0';

    /* gata! scriem serverului ca toata lumea e gata */
    if (!disconnect)
    {
      if (write(sd, msg, strlen(msg)) <= 0)
      {
        perror("[client]Eroare la write() spre server.\n");
        return errno;
      }
    }
    else
    {
      return 0;
    }
    if (strstr(msg, "stop"))
    {
      /* poate jucatorii s-au razgandit */
      printf("Ne pare rau ca nu mai doriti sa jucati. La revedere!\n");
      close(sd);
    }
  }
  else if (strstr(msg, "Introduceti un id") != NULL)
  {
    /* nu s-a dorit crearea unei camere => */
    /* asteptam un id valid */
    bzero(msg, 500);
    printf("Id: ");
    fflush(stdout);
    if (!disconnect)
    {
      read(0, msg, 500);
    }
    else
    {
      return 0;
    }
    msg[strlen(msg)] = '\0';

    /* trimitem id-ul camerei in care vrem sa intram */
    if (!disconnect)
    {
      if (write(sd, msg, strlen(msg)) <= 0)
      {
        perror("[client]Eroare la write() spre server.\n");
        return errno;
      }
    }
    else
    {
      return 0;
    }

    /* primim raspuns: am intrat in camera sau nu? */
    bzero(msg, 500);
    if (!disconnect)
    {
      if (read(sd, msg, sizeof(msg)) < 0)
      {
        perror("[client]Eroare la read() de la server.\n");
        return errno;
      }
    }
    else
    {
      return 0;
    }
    /* afisam raspunsul */
    printf("%s\n", msg);
    fflush(stdout);
  }

  /* jocul propriu-zis */
  for (int i = 1; i <= 5; i++)
  {
    in_the_game = true;
    /* citim intrebarile */
    bzero(msg, 500);
    if (!disconnect)
    {
      if (read(sd, msg, sizeof(msg)) < 0)
      {
        perror("[client]Eroare la read() de la server.\n");
        return errno;
      }
    }
    else
    {
      return 0;
    }

    /* afisam intrebarile */
    printf("%s\n", msg);
    fflush(stdout);
    bzero(answer, 255);

    printf("Raspunsul dumneavoastra: ");
    fflush(stdout);
    bzero(msg, 500);

    /* trimitem serverului mesaj ca din acest moment */
    /* clientul are 15 secunde sa raspunda */
    strcat(msg, "startingnow");
    if (!disconnect)
    {
      if (write(sd, msg, strlen(msg)) <= 0)
      {
        perror("[client]Eroare la write() spre server.\n");
        return errno;
      }
    }
    else
    {
      return 0;
    }

    /* functia care va folosi select pentru a citi */
    /* in max 15 sec raspunsurile, altfel se trimite ' ' */
    handleAnswers();

    /* trimitem raspunsul */
    answer[strlen(answer)] = '\0';
    if (!disconnect)
    {
      if (write(sd, answer, strlen(answer)) < 0)
      {
        perror("[client]Eroare la read() de la server.\n");
        return errno;
      }
    }
    else
    {
      return 0;
    }
    bzero(answer, 255);

    /* citim verdictul: corect/incorect */
    bzero(msg, 500);
    if (!disconnect)
    {
      if (read(sd, msg, sizeof(msg)) < 0)
      {
        perror("[client]Eroare la read() de la server.\n");
        return errno;
      }
    }
    else
    {
      return 0;
    }

    /* afisam verdictul */
    system("clear");
    printf("%s\n", msg);
    fflush(stdout);
  }

  /* primim punctajul nostru */
  bzero(msg, 500);
  if (!disconnect)
  {
    if (read(sd, msg, sizeof(msg)) < 0)
    {
      perror("[client]Eroare la read() de la server.\n");
      return errno;
    }
  }
  else
  {
    return 0;
  }

  /* afisam punctajul nostru */
  printf("%s\n", msg);
  fflush(stdout);
  bzero(answer, 255);
  strcat(answer, "ok");

  /* trimitem serverului mesaj ca am primit raspunsul si */
  /* ca suntem gata sa primim si clasamentul (top 3) */
  if (!disconnect)
  {
    if (write(sd, answer, strlen(answer)) < 0)
    {
      perror("[client]Eroare la read() de la server.\n");
      return errno;
    }
  }
  else
  {
    return 0;
  }

  /* citim clasamentul */
  bzero(msg, 500);
  if (!disconnect)
  {
    if (read(sd, msg, sizeof(msg)) < 0)
    {
      perror("[client]Eroare la read() de la server.\n");
      return errno;
    }
  }
  else
  {
    return 0;
  }

  /* afisam clasamentul */
  printf("%s\n", msg);
  fflush(stdout);
  bzero(answer, 255);

  /* trimitem raspunsul serverului sa-i zicem ca totul e ok */
  strcat(answer, "ok");
  if (!disconnect)
  {
    if (write(sd, answer, strlen(answer)) < 0)
    {
      perror("[client]Eroare la read() de la server.\n");
      return errno;
    }
  }
  else
  {
    return 0;
  }

  /* inchidem conexiunea, am terminat */
  close(sd);
}