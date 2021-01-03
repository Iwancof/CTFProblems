# 構造
このプログラムの構造を調べる。
どうやら、BaseMemoというクラスを基底クラスとして、show, edit, add, デストラクタ の４つが仮想関数で実装されている。基底クラスには10個分のlongの配列があり、-1で初期化している。
継承先はSumMemo, ProdMemo, StringMemo がある。
- SumMemo
	- show がメモすべての総和(しかし下位2byteしかわからない)
	- 結果はmemo[0] に保存される。
- ProdMemo
	- SumMemo の同様。こちらは総積
- StringMemo
	- 内部で0x19分のバッファを確保して、確保時のみ書き込み可能。

以上のクラス一つ分を 'page' とよび、std:;vector<BaseMemo*> な pages に保存される。
アクセスはatを使っているため、page自体のdouble-freeは不可能

# 脆弱性
脆弱性は get\_unuse\_index と BaseMemo::debug, StringMemo::add に存在する。
get\_unuse\_indexは、最初の未使用memo(つまり-1)を探すが、見つからなかったら -1 を返す。一方その関数を使っているところではそれを無視してアクセスしている。つまり、メモをいっぱいにしてaddしたら、memo[-1]に書き込まれることになる。これはvtableに該当する。
次に BaseMemo::debug の脆弱性だが、これは前述のget\_unuse\_indexのテスト関数である。未使用ブロックを返すはずなので-1が期待されるが、先ほどと同様 -1 が帰ってくるとその場が出力される。これを利用してbss領域をleakすることができる。
最後にStringMemo::addは、単純にread関数を使っているせいでNULL-terminateが済んでおらず、0x18バイト分メモリをリークすることが可能になる。

# Exploit
全体の流れとしては、
- bss の leak
- vtable の書き換え
- fake chunk の free
- chunk overlap による libc のリーク
- tcache の未初期化による heap のリーク
- tcache と fastbin による double free からの AAW
- \_\_free\_hook の書き換えによるシェルの奪取

## bss の leak
SumMemoを10個埋めた上でdebugを呼び出すとbssがleakできる。

## vtable の書き換え
vtable とはC++でポリモーフィズムを実現するための機構の一つである。
継承先のインスタンスをベースクラスとして保存したあとに仮想関数を実行しても、インスタンス固有の関数が呼び出すことができるために実装されている。今回でいえば pages が BaseMemo\* なコンテナで実装されているにも関わらず、editやadd, showをする時に固有の特徴が発現しているのはこのためである。各継承先には固有のvtableを持っており、クラスの先頭にそのtableへのポインタを持っている。
```
             +-----------------------+
             | 0    prev size        |
             | 0x61 malloc header    |
BaseMemo --> | &base\_memo\_vtable |
             | memo[0]               |
             | memo[...]             |
             | memo[9]               |
             +-----------------------+
             | 0    prev size        |
             | 0x61 malloc header    |
SumMemo ---> | &sum\_memo\_vtable  |
             | memo[0]               |
             | memo[...]             |
             | memo[9]               |
             +-----------------------+
```
と言った具合。base\_memo\_vtable の先には、
```
BaseMemo::show
BaseMemo::edit
BaseMemo::add
```
という感じで並んでいる。editを呼び出したときは、vtableからのオフセット0x8をずらしてそこでcallする。

ここで、get\_unuse\_index は埋まっていると -1 を返すので、StringMemo で 10 回addし、最後のaddではアドレスがvtableに書き込まれる。
そこで、その状態でSumMemoの内容を送ると、以降このStringMemoはSumMemoとして振る舞うことになる。

## fake chunk のfree
次に、大きなfake chunk をfreeしてlibcをleakする。
StringMemoに適当なfake chunkをmemo[0]に書き込んでおく。例えば0x500のサイズを持つchunkとか。
次に、先程説明したようにvtableをSumMemoに変え、SumMemoとして振る舞わせ、memo[1]=0x10,memo[2~9]=0としてshowする。
すると、memo[0]にはmallocした領域+0x10が書き込まれ、memo[1]=0に修正しておく。
現在のmemoは以下の通りである。
 +------------------+
 | 0: malloc()+0x10 |
 +------------------+
 | 1:      0        |
 +------------------+
 | 2~9:    0        |
 +------------------+

 ここで、0はused(-1でない)ため、次にaddしたらvtableが書き換わる。ここで、leak しておいた bss を用いて、StringMemo の vtable を書き込むと、自身がStringMemo だと思い込むようになる。

ここでそのStringMemoをdeleteすると、内部のポインタがすべてfreeされる。ここで、free(NULL)はなにもしないため、最初の fake chunk だけが free される。
0x500はunsortedbinに分類されるため、nextとnextのnextが見られるので、予め大量にPageを作成していい感じのところにprev\_inuseを立てたfake chunkを作成しておく。

## chunk overlap による libc のリーク
tcacheやfastbinがないときにmallocが走ると firstfit-allocater として振る舞うため unsortedbin が帰ってくる。ここで libc がリークできる。

## tcache の未初期化による heap のリーク
tcache をfreeしてmallocしてshowするとそこにアドレスが乗っているのでleakできる。

## tcache と fastbin による double free からの AAW
最後にAAWをする準備をする。
heap がリークできているため、前述の手法を用いて任意アドレスをfreeできる。
AAWの周辺にfreeできる箇所を見つけるのは現実できではないため、double-free を用いる。
tcache を7個埋め、fastbinでdouble-freeする。そこでtcacheを空にすると余っているfastbinを手繰ってtcacheに移動する(個数は2だけどなんか2回以上取ってこれる)。
最初のfastbinを取ってきたときに、fdに該当する箇所(vtableのptr)に、heapのアドレスを用いてsafe-linkingを回避しつつlibcのleak上をtcacheとして取ってきて、そこを\_\_free\_mallocを書き換えsystemにする。

- \_\_free\_hook の書き換えによるシェルの奪取

最後に /bin/sh\0 をfreeしてシェルが起動する。
