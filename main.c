//bashライクなシェルを作る
#include <stdio.h>     //perror
#include <stdlib.h>    //exit
#include <string.h>    //strlen
#include <unistd.h>    //exec
#include <stdarg.h>    //va_list
#include <fcntl.h>     //open
#include <sys/types.h> //wait
#include <signal.h>    //kill
#include <sys/stat.h>  //open
#include <wait.h>      //wait
#include <errno.h>

#define RDIR_IN (10)
//#define RDIR_HERE (11)
#define RDIR_OUT (20)
#define RDIR_APPEND (21)

//登録されている場合のみ行うため
#define DUP2(xx, yy)              \
    if ((xx) >= 0)                \
    {                             \
        if (dup2((xx), (yy)) < 0) \
        {                         \
            perror("dup2");       \
        }                         \
    }
#define CLOSE(xx)            \
    if ((xx) >= 0)           \
    {                        \
        if (close(xx) != 0)  \
        {                    \
            perror("close"); \
        }                    \
    }

struct cmdrec
{
    pid_t pid;         //プロセスid
    int status;        //waitのステータス
    char **cmdarg;     //コマンド引数
    int ifd;           //inファイルディスクリプタ
    int ofd;           //outファイルディスクリプタ
    int inno;          //読み込みファイルの種別
    int outno;         //書き込みファイルの種別
    char inname[256];  //読み込みファイル名
    char outname[256]; //書き込みファイル名
};

//ファイル名判定関数
int filenameacceptable(int c)
{
    int r = 0;
    if (c >= 'A' && c <= 'Z')
        r = 1;
    else if (c >= 'a' && c <= 'z')
        r = 1;
    else if (c >= '0' && c <= '9')
        r = 1;
    else if (c >= '.' || c <= '-')
        r = 1;
    return r;
}
//リダイレクト検出関数
int pickclearrdirIN(char *dst, int dlen, char *src)
{
    char *p, *q, *s;
    int c, m = 0;
    p = src; //command
    q = dst; //ファイル名
    s = NULL;
    c = 0;
    *q = '\0';
    while (*p)
    {
        if (*p == '<')
        {
            s = p;
            m = RDIR_IN;
            p++;
            // if (*p == '<')
            // {
            //     m = RDIR_HERE;
            //     p++;
            // }
            while (*p && (*p == ' ' || *p == '\t'))
                p++;
            while (*p && filenameacceptable(*p))
            {
                *q++ = *p++;
                c++;
            }
            *q = '\0';
            while (s < p) //空白でリダイレクト処理部分塗りつぶし
            {
                *s++ = ' ';
            }
            break;
        }
        p++;
    }
    return m;
}
int pickclearrdirOUT(char *dst, int dlen, char *src)
{
    char *p, *q, *s;
    int c, m = 0;
    p = src;
    q = dst; //ファイル名
    s = NULL;
    c = 0;
    *q = '\0';
    while (*p)
    {
        if (*p == '>')
        {
            s = p;
            m = RDIR_OUT;
            p++;
            if (*p == '>') //追記
            {
                m = RDIR_APPEND;
                p++;
            }
            while (*p && (*p == ' ' || *p == '\t'))
                p++;
            while (*p && filenameacceptable(*p))
            {
                *q++ = *p++;
                c++;
            }
            *q = '\0';
            while (s < p) //空白でリダイレクト処理部分塗りつぶし
            {
                *s++ = ' ';
            }
            break;
        }
        p++;
    }
    return m;
}

//連続空白を1つにする関数
void spacesone(char *dst, char *src)
{
    int ms = 0, md = 0, mb = 0, count = 0;
    char *p, *q;
    p = src;
    q = dst;
    for (count = 0; (*q++ = *p++) != '\0'; count++) //ソースが最後になるまでコピー
    {
        if (*p == '\'')
            ms = 1 - ms;
        if (*p == '\"')
            md = 1 - md;
        if (*p == '`')
            mb = 1 - mb;
        //区切り文字でない引用符中でない
        if (ms == 0 && md == 0 && mb == 0 && *p != '|')
        {
            if (*(p - 1) == ' ') //空白
            {
                while (*p == ' ') //空白じゃなくなるまでポインタ進める．
                    p++;
            }
        }
    }
    //最後の処理
    if (dst[count - 1] == ' ')
        dst[count - 1] = '\0';
    else
        dst[count] = '\0';
    //最初の処理
    if (*dst == ' ')
        memmove(dst, dst + 1, count + 1);
}

//区切る
char **allocsplit(char *src, int del, ...)
{
    int fig = 0, line = 1, delN, delF;
    char **out, *p = src;
    int dels = del, delc[BUFSIZ];

    va_list args;
    //区切り文字格納
    va_start(args, del);
    for (delN = 0; dels != 0; delN++)
    {
        delc[delN] = dels;
        dels = va_arg(args, int); //次の区切り文字
    }
    va_end(args);
    //配列の必要行数確認
    while (*p != '\0') //終端まで見る
    {
        delF = 0;                      //初期化
        for (int i = 0; i < delN; i++) //区切り探し
        {
            if (*p == delc[i])
            {
                delF = 1;
            }
        }
        if (delF == 1) //区切り文字有
        {
            line++;
        }
        *p++; //次の文字
    }
    p = src; //初期化
    //動的確保(行)
    out = (char **)malloc(sizeof(char *) * line);
    if (!out)
    {
        perror("malloc");
        exit(1);
    }
    //動的確保(要素数)
    for (int i = 0; i < line; i++) //改行の文字数分
    {
        for (fig = 0; *p; fig++)
        {
            delF = 0;                      //初期化
            for (int i = 0; i < delN; i++) //区切り探し
            {
                if (*p == delc[i])
                {
                    delF = 1;
                }
            }
            if (delF == 1) //区切り文字有
            {
                break;
            }
            *p++;
        }
        out[i] = (char *)malloc(sizeof(char) * fig);
        if (!out[i])
        {
            perror("malloc");
            exit(1);
        }
        *p++; //del飛ばし
    }
    p = src; //初期化
    //代入
    for (int i = 0, j; i < line; i++)
    {
        for (j = 0; *p; j++)
        {
            delF = 0;                      //初期化
            for (int k = 0; k < delN; k++) //区切り探し
            {
                if (*p == delc[k])
                {
                    delF = 1;
                }
            }
            if (delF == 1) //区切り文字有
            {
                break;
            }
            out[i][j] = *p++;
        }
        p++;              //del飛ばし
        out[i][j] = '\0'; //終端記号
    }
    out[line] = (char *)0;
    return out;
}

int main(void)
{
    int n = -1;
    char *comhist[BUFSIZ];
    while (1)
    {
        int comN, incom = 0; //内部コマンド判定子
        char input[BUFSIZ], buf[BUFSIZ], **inppipe;
        char pathname[BUFSIZ];

        if (getcwd(pathname, BUFSIZ) == NULL)
            perror("getcwd");
        printf("%s[%d]: ", pathname, ++n);
        /* 1行読み込む */
        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            if (errno != 0)
                perror("fgets");
            /* EOF(Ctrl-D)が入力された */
            printf("See you agein.\n");
            break;
        }
        input[strlen(input) - 1] = '\0'; //改行コード削除

        comhist[n] = (char *)malloc(sizeof(char) * strlen(input) + 1);
        if (!comhist[n])
        {
            perror("malloc");
            exit(1);
        }
        for (int i = 0; input[i]; i++)
        {
            comhist[n][i] = input[i];
        }
        /* 連続空白を一つに */
        spacesone(buf, input);
        inppipe = allocsplit(buf, '|', 0); //パイプで区切る

        //コマンド数確認
        for (comN = 0; inppipe[comN]; comN++)
        {
            if (strcmp(inppipe[comN], "exit") == 0)
            {
                printf("See you agein.\n");
                incom = 1;
                return (0);
            }
            // printf("%3d:|%s|\n", comN, inppipe[comN]);
        }

        struct cmdrec *cmdar;
        cmdar = (struct cmdrec *)malloc(sizeof(struct cmdrec) * comN);
        if (!cmdar)
        {
            perror("malloc");
            exit(1);
        }
        //空白で区切る
        for (int i = 0; i < comN; i++)
        {

            //リダイレクト処理
            cmdar[i].inno = pickclearrdirIN(cmdar[i].inname, '\0', inppipe[i]);
            cmdar[i].outno = pickclearrdirOUT(cmdar[i].outname, '\0', inppipe[i]);
            spacesone(buf, inppipe[i]); //リダイレクト処理後の空白部分の処理
            cmdar[i].cmdarg = allocsplit(buf, ' ', 0);

            //内部コマンドパート
            if (strcmp(cmdar[i].cmdarg[0], "history") == 0)
            {
                for (int i = 0; i < n; i++)
                {
                    printf("%d\t%s\n", i, comhist[i]);
                }
                incom = 1;
            }
            if (strcmp(cmdar[i].cmdarg[0], "cd") == 0)
            {
                if (cmdar[i].cmdarg[1])
                {
                    // カレントディレクトリ変更
                    if (chdir(cmdar[i].cmdarg[1]) < 0) // チェンジディレクトリ
                        perror("chdir");
                    incom = 1;
                }
                else
                {
                    printf("cd:Invalid argument\n");
                }
            }
            if (strcmp(cmdar[i].cmdarg[0], "kill") == 0)
            {
                if (cmdar[i].cmdarg[1])
                {
                    int killno = strtol(cmdar[i].cmdarg[1], NULL, 10);
                    if (killno < 1)
                    {
                        perror("strtoll");
                    }
                    else
                    {
                        if (kill(killno, SIGTERM) < 0)
                            perror("kill");
                        incom = 1;
                    }
                }
                else
                {
                    printf("kill:Invalid argument\n");
                }
            }
        }

        //外部コマンド処理
        if (incom != 1)
        {
            // パイプ登録
            for (int i = comN - 1; i >= 0; i--) //先に後ろから
            {
                //マクロで判定できるように初期化
                cmdar[i].ifd = -1;
                if (i == comN - 1)
                    cmdar[i].ofd = -1;
                if (i > 0)
                {
                    int fd[2];
                    if (pipe(fd) == -1)
                    {
                        perror("pipe");
                        exit(1);
                    }
                    cmdar[i].ifd = fd[0];
                    cmdar[i - 1].ofd = fd[1];
                }
                if (cmdar[i].inno == RDIR_IN)
                {
                    if ((cmdar[i].ifd = open(cmdar[i].inname, O_RDONLY)) < 0)
                        perror("open");
                }
                // else if (cmdar[i].inno == RDIR_HERE)
                // {
                //     if ((cmdar[i].ifd = open(cmdar[i].inname, /*ここ*/)) == -1)
                //         perror("open");
                // }
                if (cmdar[i].outno == RDIR_OUT)
                {
                    if ((cmdar[i].ofd = creat(cmdar[i].outname, 0644)) < 0)
                        perror("creat");
                }
                else if (cmdar[i].outno == RDIR_APPEND)
                {
                    if ((cmdar[i].ofd = open(cmdar[i].outname, O_WRONLY | O_APPEND | O_CREAT, 0644)) < 0)
                        perror("open");
                }
            }
            // パイプ不要
            if (comN == 1)
            {
                for (int i = comN - 1; i >= 0; i--) //先に後ろから
                {
                    //プロセス立ち上げ
                    cmdar[i].pid = fork();
                    if (cmdar[i].pid < 0) //エラー
                    {
                        perror("fork");
                        exit(1);
                    }
                    else if (cmdar[i].pid == 0) //子プロセス
                    {
                        DUP2(cmdar[i].ifd, STDIN_FILENO);
                        DUP2(cmdar[i].ofd, STDOUT_FILENO);
                        for (int j = 0; j < comN; j++)
                        {
                            CLOSE(cmdar[j].ifd);
                            CLOSE(cmdar[j].ofd);
                        }
                        if (execvp(cmdar[i].cmdarg[0], cmdar[i].cmdarg) < 0)
                            perror("execvp");
                        exit(0);
                    }
                    else //親プロセス
                    {
                        for (int j = 0; j < comN; j++)
                        {
                            CLOSE(cmdar[j].ifd);
                            CLOSE(cmdar[j].ofd);
                        }
                        if (waitpid(cmdar[i].pid, &cmdar[i].status, 0) < 0)
                            perror("waitpid");
                    }
                }
            }
            //パイプ処理
            for (int i = 0; i < comN && comN != 1; i++)
            {
                cmdar[i].pid = fork();
                if (cmdar[i].pid == 0)
                {
                    if (i == 0) //先頭
                    {
                        DUP2(cmdar[i].ifd, STDIN_FILENO);
                        DUP2(cmdar[i].ofd, STDOUT_FILENO);
                        CLOSE(cmdar[i].ifd);
                        CLOSE(cmdar[i].ofd);
                        CLOSE(cmdar[i + 1].ifd);
                    }
                    else if (i == comN - 1) //最後
                    {
                        DUP2(cmdar[i].ifd, STDIN_FILENO);
                        DUP2(cmdar[i].ofd, STDOUT_FILENO);
                        CLOSE(cmdar[i].ifd);
                        CLOSE(cmdar[i].ofd);
                        CLOSE(cmdar[i - 1].ofd);
                    }
                    else //中間
                    {
                        DUP2(cmdar[i].ifd, STDIN_FILENO);
                        DUP2(cmdar[i].ofd, STDOUT_FILENO);
                        CLOSE(cmdar[i - 1].ofd);
                        CLOSE(cmdar[i].ifd);
                        CLOSE(cmdar[i].ofd);
                        CLOSE(cmdar[i + 1].ifd);
                    }
                    execvp(cmdar[i].cmdarg[0], cmdar[i].cmdarg);
                    exit(0);
                }
                else if (i > 0) //全閉
                {
                    CLOSE(cmdar[i].ifd);
                    CLOSE(cmdar[i - 1].ofd);
                    if (waitpid(cmdar[i].pid, &cmdar[i].status, 0) < 0)
                        perror("waitpid");
                }
            }
        }
    }
    return 0;
}
