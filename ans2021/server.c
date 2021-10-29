#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

#define ITEM_LENGTH 5
#define COIN_LENGTH 5
#define MSG_SIZE 1024

int checkInputMoney(char *input);
//int socWrite(int *ws, char *message, int *msg_len);
enum
{
    SERVER_PORT = 39999,
    NQUEUESIZE = 5,
    MAXNCLIENTS = 10,
};

int clients[MAXNCLIENTS];
int nclients = 0;

void sorry(int ws)
{
    char *message = "Sorry, It's full of customers now.\n";
    write(ws, message, strlen(message));
}

void delete_client(int ws)
{
    int i;

    for (i = 0; i < nclients; i++)
    {
        if (clients[i] == ws)
        {
            clients[i] = clients[nclients - 1];
            nclients--;
            break;
        }
    }
}

//終了動作
void finish(int ws)
{
    shutdown(ws, SHUT_RDWR);
    close(ws);
    delete_client(ws);
    fprintf(stderr, "Connection closed on descriptor %d.\n", ws);
    //fdclose(ws);
}

//入力された金額が、硬貨、紙幣として存在するかどうか
int checkInputMoney(char *input)
{
    char *coin[] = {"10", "50", "100", "500", "1000"};
    int j;
    for (j = 0; j < COIN_LENGTH; j++)
    {
        if (strcmp(input, coin[j]) == 0)
        {
            fprintf(stderr, "%s円は使用可能です\n", input);
            return 0;
        }
    }
    fprintf(stderr, "Money Error.");
    return 1;
}

//購入商品受付
void showItems(int ws, char **item_arr, int *price_arr)
{
    int i;
    char message[1024];

    sprintf(message, "\nいらっしゃいませ\n\n");
    write(ws, message, strlen(message));

    for (i = 0; i < ITEM_LENGTH; i++)
    {
        sprintf(message, "%s(%d)", item_arr[i], price_arr[i]);
        write(ws, message, strlen(message));
    }

    sprintf(message, "\n\n購入商品を入力してください:");
    write(ws, message, strlen(message));
}

//購入商品検索
int getItemName(int ws, char **item_arr, int *price_arr, int *stock, int *buy_item)
{
    int i, cc;
    char c;
    char item[10];
    char message[1024];

    //fprintf(stderr, "ws:%d\n", ws);

    if ((cc = read(ws, &item, sizeof(item))) == -1)
    {
        perror("read");
        exit(1);
    }
    else if (cc == 0)
    {
        finish(ws);
        return 0;
    }

    int err = 0;
    for (int i = 0; i < cc; i++)
    {
        if (iscntrl(item[i]))
        {
            item[i] = '\0';
            printf("%d\n", i);
            err = 1;
        }
    }
    if (err == 0)
    {
        sprintf(message, "入力文字数が多すぎます.\nもう一度やり直してください.\n");
        write(ws, message, strlen(message));
        finish(ws);
        return 0;
    }
    // fprintf(stderr, "入力:%s\n", &item[5]);

    fprintf(stderr, "入力:%s(%d)\n", item, cc);

    if (cc == 3)
    {
        sprintf(message, "終了します\n\n");
        write(ws, message, strlen(message));
        finish(ws);
        return 0;
    }

    for (i = 0; i < ITEM_LENGTH; i++)
    {
        if (strcmp(item, item_arr[i]) == 0)
        {
            buy_item[ws] = i;
            int price;
            price = price_arr[i];
            sprintf(message, "%sの値段は", item);
            write(ws, message, strlen(message));

            sprintf(message, "%d", price_arr[i]);
            write(ws, message, strlen(message));

            sprintf(message, "円です\n\n投入金額(0/%d):", price_arr[i]);
            write(ws, message, strlen(message));
            return price_arr[i];
        }
        else if (i == ITEM_LENGTH - 1)
        {
            sprintf(message, "その商品は存在しません\n\n");
            write(ws, message, strlen(message));
            fprintf(stderr, "No Item Error.");
            return 1;
        }
    }
}

int payMoney(int ws, int *price, int *pay)
{
    int i, cc;
    char c;
    char coin[5];
    char message[1024];
    int check;
    int money;
    money = pay[ws];

    if ((cc = read(ws, &coin, sizeof(coin))) == -1) /*read*/
    {
        perror("read");
        exit(1);
    }
    else if (cc == 0)
    {
        finish(ws);
        return 0;
    }

    int err = 1;
    for (int i = 0; i < cc; i++)
    {
        if (iscntrl(coin[i]))
        {
            coin[i] = '\0';
            printf("%d\n", i);
            err = 0;
        }
    }
    if (err == 1)
    {
        sprintf(message, "入力文字数が多すぎます.\nもう一度やり直してください.\n");
        write(ws, message, strlen(message));
        finish(ws);
        return 0;
    }

    if (strcmp(coin, "x") == 0)
    {
        sprintf(message, "購入を取り消します\n\n");
        write(ws, message, strlen(message));
        fprintf(stderr, "Stop buying...\n");
        finish(ws);
        return 0;
    }
    check = checkInputMoney(coin);
    if (check == 0)
    {
        money += atoi(coin); //入金加算
        //memset(&coin, 0, sizeof(coin));
        return money;
    }
    else
    {
        sprintf(message, "そのお金は使用できません\n");
        write(ws, message, strlen(message));
        sprintf(message, "使用可能なお金は1000円,500円,100円,50円,10円です\n");
        write(ws, message, strlen(message));
        return 1;
    }
}

int main(void)
{
    /*自販機用*/
    char *item_arr[] = {"apple", "coffee", "milk", "orange", "tea"};
    int price_arr[] = {70, 100, 40, 80, 50};
    int stock[] = {5, 4, 3, 2, 1};

    int i;
    int change = 0;
    int msg_len = 0;
    int state[MAXNCLIENTS];
    int buy_item[MAXNCLIENTS];

    char message[MSG_SIZE];
    char item[10];
    char buf[20];

    int price[MAXNCLIENTS];
    int pay[MAXNCLIENTS];

    /*ソケット通信用*/ //ws
    int s, soval;
    struct sockaddr_in sa;

    /*socket*/
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }

    /*アドレス再利用設定*/
    soval = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &soval, sizeof(soval)) == -1)
    {
        perror("setsockopt");
        exit(1);
    }

    /*ソケットに名付け(for bind)*/
    memset(&sa, 0, sizeof(sa));
    //sa.sin_len = sizeof(sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(SERVER_PORT);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    /*bind*/
    if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) == -1)
    {
        perror("bind");
        exit(1);
    }

    /*listen*/
    if (listen(s, NQUEUESIZE))
    {
        perror("listen");
        exit(1);
    }

    /*Ready*/
    fprintf(stderr, "Ready.\n");

    /*入力待ち*/
    for (;;)
    {
        int i;
        int maxfd;      //調べるファイル記述子中で最大のもの
        fd_set readfds; //入力用のファイル記述子の集合

        /*ファイル記述子の集合を作成*/
        FD_ZERO(&readfds);
        FD_SET(s, &readfds);
        maxfd = s;

        /*クライアント見張り*/
        fprintf(stderr, "Waiting for a connection...\n");
        for (i = 0; i < nclients; i++)
        {
            FD_SET(clients[i], &readfds);
            if (clients[i] > maxfd)
                maxfd = clients[i];
        }

        /*select*/
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0)
        {
            perror("select");
            exit(1);
        }

        /*新規接続*/
        int ws;
        if (FD_ISSET(s, &readfds))
        {
            struct sockaddr_in ca;
            socklen_t ca_len;

            ca_len = sizeof(ca);
            /*accept*/
            if ((ws = accept(s, (struct sockaddr *)&ca, &ca_len)) == -1)
            {
                perror("accept");
                continue;
                fprintf(stderr, "Connection established.\n");
            }
            if (nclients >= MAXNCLIENTS)
            {
                sorry(ws);
                shutdown(ws, SHUT_RDWR);
                close(ws);
                fprintf(stderr, "Refused a new connection.\n");
            }
            else
            {
                clients[nclients] = ws;
                nclients++;
                fprintf(stderr, "Accepted a connection on descriptor %d.\n", ws);
            }

            // if ((fp= fdopen (s, "r+")) == NULL) { /* read/write */
            //     perror ("fdopen"); exit (1);
            // }
            // setvbuf(buf, _IOLBF, BUFSIZE);

            showItems(ws, item_arr, price_arr);
            state[ws] = 0;
            fprintf(stderr, "state[%d]:%d\n", ws, state[ws]);
        }

        for (i = 0; i < nclients; i++)
        {
            if (FD_ISSET(clients[i], &readfds))
            {
                fprintf(stderr, "state[%d]:%d\n", clients[i], state[clients[i]]);
                if (state[clients[i]] == 0)
                {
                    fprintf(stderr, "ws:%d,client:%d, i:%d(%d).\n", ws, clients[i], i, nclients);
                    int j;
                    j = getItemName(clients[i], item_arr, price_arr, stock, buy_item);
                    fprintf(stderr, "%d(%d).\n", j, clients[i]);
                    if (j == 1)
                    {
                        showItems(clients[i], item_arr, price_arr);
                        break;
                    }
                    else if (j != 0)
                    {
                        fprintf(stderr, "%d(%d).\n", j, clients[i]);
                        state[clients[i]] = 1; //状態保存
                        price[clients[i]] = j; //購入商品の値段
                        pay[clients[i]] = 0;   //初期化
                        fprintf(stderr, "state:%d(%d)\n", state[clients[i]], clients[i]);
                    }
                }
                else if (state[clients[i]] == 1)
                {
                    int a;
                    a = payMoney(clients[i], price, pay);
                    if (a != 0 && a != 1)
                    {
                        pay[clients[i]] = a;
                        fprintf(stderr, "%d,%d(%d).\n", pay[clients[i]], price[clients[i]], clients[i]);
                        if (pay[clients[i]] < price[clients[i]])
                        {
                            fprintf(stderr, "%d,%d(%d).\n", pay[clients[i]], price[clients[i]], clients[i]);
                            sprintf(message, "投入金額(%d/%d):", pay[clients[i]], price_arr[i]);
                            write(clients[i], message, strlen(message));
                        }
                        else
                        {
                            state[i] = 2;

                            /*購入完了処理*/
                            sprintf(message, "\n投入金額合計：%d円\n\n%sのお渡しです！", pay[clients[i]], item_arr[buy_item[clients[i]]]);
                            write(clients[i], message, strlen(message));
                            if (pay[clients[i]] > price[clients[i]])
                            {
                                change = pay[clients[i]] - price[clients[i]];
                                sprintf(message, "\n\nおつり：%d円", change);
                                write(clients[i], message, strlen(message));
                            }
                            sprintf(message, "\n\n購入ありがとうございました!\n\n");
                            write(clients[i], message, strlen(message));

                            finish(clients[i]);
                        }
                    }
                    else if (a == 1)
                    {
                        sprintf(message, "投入金額(%d/%d):", pay[clients[i]], price_arr[i]);
                        write(clients[i], message, strlen(message));
                    }
                }
            }
        }
    }
}
