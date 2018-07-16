# ShellByC
C言語でBashをつくる．
任意個のパイプとリダイレクト処理が行える．
外部コマンドはexecができる範囲で使用できる．

対応している内部コマンドは以下．
cd [dir]	カレントディレクトリをdirに変更
例）
/tmp[1]: cd hoge
/tmp/hoge[2]:

history	行番号付きでコマンド履歴を表示
例）
/tmp[4]: history
0       cd /tmp
1       ls
2       rm hoge
3       rm -rf hoge

kill [pid | jobspec]	[pid | jobspec]で指定されたプロセスにSIGTERMシグナルを送信
例）
/tmp[2]: ps -a
  PID TTY          TIME CMD
    4 tty1     00:00:00 bash
  433 tty1     00:00:00 main
  436 tty2     00:00:00 bash
  520 tty2     00:00:00 find
  521 tty1     00:00:00 ps
/tmp[3]: kill 520
/tmp[4]: ps -a
  PID TTY          TIME CMD
    4 tty1     00:00:00 bash
  433 tty1     00:00:00 main
  436 tty2     00:00:00 bash
  522 tty1     00:00:00 ps

exit	シェルを終了
例）
Ubuntu@Surface-Kaito:/tmp$ ~/i442/kadai3/main
/tmp[0]: exit
See you agein.
Ubuntu@Surface-Kaito:/tmp$
