#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>

/*

    食物連鎖のピラミッドがバランスを保つ様子を
    シュミレートしたい。

    フィールド上の炭素量が一定になるように、
    grass, cow, lion の三種の個体の食物連鎖を考える。

    cowはgrassを,lionはcowを食べ、grassは動物の死骸を栄養とする。(炭素の移動)
    それぞれの個体はフィールド場でランダムに動き回る。(grassは動かない)
    個体は一ステップで周りとその場所の9箇所のいずれかに移動する。

    捕食者は被捕食者と同じ場所に移動した時,1捕食者につき1被捕食者を捕食する。
    (同じ場所に同種族の複数の個体が存在しうる。)
    捕食は炭素の移動であり炭素は種族全体で共有するものとする。(簡易モデル)

    種族で共有する炭素数を種族の個体数で割った数(整数値)を一個体炭素数と定義する。
    捕食とは被捕食者の一個体炭素数を捕食者の種族の共有する炭素に移動することである。

    一個体炭素数が一定以上になった時、ランダムな場所で新たなその種の個体が誕生する。
    一個体炭素数が一定以下になった時、その種のランダムな個体が死滅する。

    老衰死として、一ステップごとにcowとlionの種族の炭素数を一定値減らす。
*/

/*

    遊び方

    下の各種パラメタはこのままで食物連鎖のバランスを取ってくれます
    （試してみたランダムシードではの話ですが）

    一番下のmain関数内で個体のバランスをある時点で崩してみると面白いです

    僕が悪いんですが、少しPCのスペック問われます

*/

// random seed
const unsigned long long SEED = 20181211;

// フィールドのサイズ
const long long FIELD_WIDTH  = 400;
const long long FIELD_HEIGHT = 400; 

// 初期状態の個体数
const long long INIT_NUM_GRASS = 20000;
const long long INIT_NUM_COW   = 1400;
const long long INIT_NUM_LION  = 200;

// 死滅、誕生基準の一個体炭素数 
// 初期の一個体炭素数がこの中間になるよう種族の炭素数を設定する
const long long UPPER_NUM_GRASS = 400;
const long long UNDER_NUM_GRASS = 200;
const long long UPPER_NUM_COW   = 4000;
const long long UNDER_NUM_COW   = 2000;
const long long UPPER_NUM_LION  = 16000;
const long long UNDER_NUM_LION  = 8000;

// 1ステップごとに老衰する分(grassに渡される炭素)
const long long DECAY_COW = 5;
const long long DECAY_LION = 20;

// View Width, Height
const long long VIEW_HEIGHT = 30;
const long long VIEW_WIDTH  = 30;

// lib
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

// field[FIELD_HEIGHT][FIELD_WIDTH][3]
// フィールドについてmain関数内で簡単に説明

// フィードの初期化
void init_field(long long field[FIELD_HEIGHT][FIELD_WIDTH][3]){
    srand(SEED);
    // 一個体ずつランダムに配置
    for(long long i = 0;i < INIT_NUM_GRASS;i ++){ // grass
        long long x = (long long)(rand()%FIELD_WIDTH);
        long long y = (long long)(rand()%FIELD_HEIGHT);
        field[y][x][0] ++;
    }
    for(long long i = 0;i < INIT_NUM_COW;i ++){ // cow
        long long x = (long long)(rand()%FIELD_WIDTH);
        long long y = (long long)(rand()%FIELD_HEIGHT);
        field[y][x][1] ++;
    }
    for(long long i = 0;i < INIT_NUM_LION;i ++){ // lion
        long long x = (long long)(rand()%FIELD_WIDTH);
        long long y = (long long)(rand()%FIELD_HEIGHT);
        field[y][x][2] ++;
    }
}

// 1000x1000は表示不可能なので中心のVIEW_HEIGHTxVIEW_WIDTHを表示
void print_field(FILE *fp,long long field[FIELD_HEIGHT][FIELD_WIDTH][3],int day){

    fprintf(fp,"\e[18E");
    fprintf(fp,"中心の一部を表示中\e[1E");
    // 上辺
    fprintf(fp,"+-");
    for(long long x = 0;x < VIEW_WIDTH;x ++){
        fprintf(fp,"--");
    }
    fprintf(fp,"-+\n");
    // 中身
    for(long long y = (FIELD_HEIGHT-VIEW_HEIGHT)/2;y < (FIELD_HEIGHT+VIEW_HEIGHT)/2;y ++){
        fprintf(fp,"| ");
        for(long long x = (FIELD_WIDTH-VIEW_WIDTH)/2;x < (FIELD_WIDTH+VIEW_WIDTH)/2;x ++){ // より強い種族を表示
            if(field[y][x][2] > 0){
                fprintf(fp,"\e[38;5;009m# \e[m");
            }else if(field[y][x][1] > 0){
                fprintf(fp,"\e[38;5;015m# \e[m");
            }else if(field[y][x][0] > 0){
                fprintf(fp,"\e[38;5;034m# \e[m");
            }else{
                fprintf(fp,"  ");
            }
        }
        fprintf(fp," |\n");
    }
    // 下辺
    fprintf(fp,"+-");
    for(long long x = 0;x < VIEW_WIDTH;x ++){
        fprintf(fp,"--");
    }
    fprintf(fp,"-+\n");

    // 位置調整 
    fprintf(fp,"\e[34F");
    fprintf(fp,"\e[%lldF",VIEW_HEIGHT);
}

// 各個体が周囲に移動、またはその場に停留する
void move(long long field[FIELD_HEIGHT][FIELD_WIDTH][3]){
    long long ***buf = (long long***)malloc(sizeof(long long**)*FIELD_HEIGHT);

    // 移動後のフィールドの一時保存先を確保
    // メモリが大きすぎるのでmallocで確保
    for(long long y = 0;y < FIELD_HEIGHT;y ++){
        buf[y] = (long long**)malloc(sizeof(long long*)*FIELD_WIDTH);
        for(long long x = 0;x < FIELD_WIDTH;x ++){
            buf[y][x] = (long long*)malloc(sizeof(long long)*3);
            for(long long i = 0;i < 3;i ++){
                buf[y][x][i] = 0;
            }
        }
    }

    // 移動
    for(long long y = 0;y < FIELD_HEIGHT;y ++){
        for(long long x = 0;x < FIELD_WIDTH;x ++){
            for(long long i = 1;i < 3;i ++){
                for(long long j = 0;j < field[y][x][i];j ++){
                    long long newx = x + rand()%3 - 1;
                    long long newy = y + rand()%3 - 1;
                    if(newx <= -1) newx = FIELD_WIDTH-1;
                    if(newx >= FIELD_WIDTH) newx = 0;
                    if(newy <= -1) newy = FIELD_HEIGHT-1;
                    if(newy >= FIELD_HEIGHT) newy = 0;
                    buf[newy][newx][i] ++;
                }
            }
        }
    }

    // bufからfieldへコピー　かつbufの開放
    for(long long y = 0;y < FIELD_HEIGHT;y ++){
        for(long long x = 0;x < FIELD_WIDTH;x ++){
            for(long long i = 1;i < 3;i ++){
                field[y][x][i] = buf[y][x][i];
            }
            free(buf[y][x]);
        }
        free(buf[y]);
    }
    free(buf);
}

// 重なっている捕食、被捕食者間のお食事タイム
void eat(long long field[FIELD_HEIGHT][FIELD_WIDTH][3],long long *grass_sum, long long *cow_sum, long long *lion_sum, long long *c_grass, long long *c_cows, long long *c_lions){
    long long c_per_grass = (*c_grass)/(*grass_sum);
    long long c_per_cow   = (*c_cows)/(*cow_sum);
    
    for(long long y = 0;y < FIELD_HEIGHT;y++){
        for(long long x = 0;x < FIELD_WIDTH;x++){
            // 各地点での
            // 各種族の数
            long long grass = field[y][x][0];
            long long cows  = field[y][x][1];
            long long lions = field[y][x][2];

            // cows eat grass
            long long amount = MIN(grass,cows); // 牛と草が重なっている数
            long long del_c = 0;
            del_c = c_per_grass*amount; // 牛が草を食べる炭素量
            *c_cows += del_c; *c_grass -= del_c; // 炭素の移動
            *grass_sum -= amount; field[y][x][0] -= amount;// 草の減少

            // lions eat cows
            amount = MIN(cows,lions); // ライオンと牛が重なっている数
            del_c = c_per_cow*amount; // ライオンが牛を食べる炭素量
            *c_lions += del_c; *c_cows -= del_c; // 炭素の移動
            *cow_sum -= amount; field[y][x][1] -= amount; // 牛の減少
        }
    }
}

// 生殖 １個体炭素数が一定値（UPPER_NUM_???）を超えないようにふえる
void reprduce(long long field[FIELD_HEIGHT][FIELD_WIDTH][3],long long *grass_sum, long long *cow_sum, long long *lion_sum, long long *c_grass, long long *c_cows, long long *c_lions){
    // grass
    int num = *c_grass/UPPER_NUM_GRASS - *grass_sum; // 増加分
    for(int j = 0;j < num;j ++){
        int x = rand()%FIELD_WIDTH;
        int y = rand()%FIELD_HEIGHT;
        field[y][x][0] ++;
        (*grass_sum) ++;
    }
    // cow
    num = *c_cows/UPPER_NUM_COW - *cow_sum; // 増加分
    for(int j = 0;j < num;j ++){
        int x = rand()%FIELD_WIDTH;
        int y = rand()%FIELD_HEIGHT;
        field[y][x][1] ++;
        (*cow_sum) ++;
    }
    // lion
    num = *c_lions/UPPER_NUM_LION - *lion_sum; // 増加分
    for(int j = 0;j < num;j ++){
        int x = rand()%FIELD_WIDTH;
        int y = rand()%FIELD_HEIGHT;
        field[y][x][2] ++;
        (*lion_sum) ++;
    }
}

// ランダムに殺害 kind 0:grass 1:cow 2:lion
void kill_in_field(long long field[FIELD_HEIGHT][FIELD_WIDTH][3], int kind){
    // ランダムな位置から殺害対象を捜査、見つけ次第殺して帰る
    for(int y = rand()%FIELD_HEIGHT;y < FIELD_HEIGHT;y ++){
        for(int x = rand()%FIELD_WIDTH;x < FIELD_WIDTH;x ++){
            if(field[y][x][kind] > 0){
                field[y][x][kind] --;
                return;
            }
        }
    }
    for(long long y = 0;y < FIELD_HEIGHT;y++){
        for(long long x = 0;x < FIELD_WIDTH;x++){
            if(field[y][x][kind] > 0){
                field[y][x][kind] --;
                return;
            }
        }
    }
}

// 餓死 １個体炭素数が一定値を下回らないように数を減らす
void hunger(long long field[FIELD_HEIGHT][FIELD_WIDTH][3],long long *grass_sum, long long *cow_sum, long long *lion_sum, long long *c_grass, long long *c_cows, long long *c_lions){
    // cow
    int num = *cow_sum - *c_cows/UNDER_NUM_COW; // 減少分
    for(int j = 0;j < num;j ++){
        kill_in_field(field,1);
        (*cow_sum) --;
    }
    // lion
    num = *lion_sum - *c_lions/UNDER_NUM_LION; // 減少分
    for(int j = 0;j < num;j ++){
        kill_in_field(field,2);
        (*lion_sum) --;
    }
}

// 老死
void decay(long long *grass_sum, long long *cow_sum, long long *lion_sum, long long *c_grass, long long *c_cows, long long *c_lions){
    // cow の老衰
    *c_cows  -= DECAY_COW*(*cow_sum);
    *c_grass += DECAY_COW*(*cow_sum);
    // lion の老衰
    *c_lions -= DECAY_LION*(*lion_sum);
    *c_grass += DECAY_LION*(*lion_sum);
}

// length の長さのbarを現カーソル位置から表示
void print_bar(FILE *fp,int length,const char *color){
    fprintf(fp,"%s",color);
    for(int i = 0;i < length;i ++){
        fprintf(fp," ");
    }
    fprintf(fp,"\e[m");
}

// 食物連鎖のピラミッドを表示
void print_pyramid(FILE *fp,long long *grass_sum, long long *cow_sum, long long *lion_sum, long long *c_grass, long long *c_cows, long long *c_lions){
    int sum = *grass_sum + *cow_sum + *lion_sum;
    int grass = *grass_sum * 150 / sum;
    int cow   = *cow_sum   * 150 / sum;
    int lion  = *lion_sum  * 150 / sum;

    fprintf(fp,"\e[2E各個体数");
    fprintf(fp,"\e[1E");
    fprintf(fp,"           :");      print_bar(fp,lion, "\e[48;5;009m");fprintf(fp,"\e[1E");
    fprintf(fp,"    LION   :");      print_bar(fp,lion, "\e[48;5;009m");fprintf(fp,"\e[1E");
    fprintf(fp,"%11lld:",*lion_sum); print_bar(fp,lion, "\e[48;5;009m");fprintf(fp,"\e[1E");
    fprintf(fp,"           :");      print_bar(fp,lion, "\e[48;5;009m");fprintf(fp,"\e[1E");
    fprintf(fp,"           :");      print_bar(fp,cow,  "\e[48;5;015m");fprintf(fp,"\e[1E");
    fprintf(fp,"    COW    :");      print_bar(fp,cow,  "\e[48;5;015m");fprintf(fp,"\e[1E");
    fprintf(fp,"%11lld:",*cow_sum);  print_bar(fp,cow,  "\e[48;5;015m");fprintf(fp,"\e[1E");
    fprintf(fp,"           :");      print_bar(fp,cow,  "\e[48;5;015m");fprintf(fp,"\e[1E");
    fprintf(fp,"           :");      print_bar(fp,grass,"\e[48;5;034m");fprintf(fp,"\e[1E");
    fprintf(fp,"   GRASS   :");      print_bar(fp,grass,"\e[48;5;034m");fprintf(fp,"\e[1E");
    fprintf(fp,"%11lld:",*grass_sum);print_bar(fp,grass,"\e[48;5;034m");fprintf(fp,"\e[1E");
    fprintf(fp,"           :");      print_bar(fp,grass,"\e[48;5;034m");fprintf(fp,"\e[2E");
    fprintf(fp,"各種族 炭素保有数:");
    fprintf(fp,"lion:%15lld  /",*c_lions);
    fprintf(fp,"cow:%15lld  /",*c_cows);
    fprintf(fp,"grass:%15lld\e[16F",*c_grass);
}

 // Ctrl^C対応　（file開けっ放しにしないように）
void abrt(int sig);
volatile sig_atomic_t e_flag = 0;

int main(int argc, char const *argv[]){
    // c_??? は炭素数　ほかは個体数
    long long grass   = INIT_NUM_GRASS;
    long long c_grass = (UPPER_NUM_GRASS+UNDER_NUM_GRASS)/2*INIT_NUM_GRASS;
    long long cows    = INIT_NUM_COW;
    long long c_cows  = (UPPER_NUM_COW  +UNDER_NUM_COW)/2*INIT_NUM_COW; 
    long long lions   = INIT_NUM_LION;
    long long c_lions = (UPPER_NUM_LION +UNDER_NUM_LION)/2*INIT_NUM_LION;
    
    FILE *fp = stdout;

    // フィールド field[y][x][0:grass, 1:cow, 2:lion]
    // 各種族のその場所にいる個体数を値にする
    static long long field[FIELD_HEIGHT][FIELD_WIDTH][3] = {0};

    init_field(field);

    system("clear");
    int day = 0;

    if( signal(SIGINT, abrt ) == SIG_ERR){
        exit(1);
    }
    // FILE *f = fopen("data.csv","w");
    while(!e_flag){
        day ++;
        fprintf(fp,"day %d",day);
        print_pyramid(fp,&grass,&cows,&lions,&c_grass,&c_cows,&c_lions);
        print_field(fp,field,day);
        move(field);
        eat(field,&grass,&cows,&lions,&c_grass,&c_cows,&c_lions);
        reprduce(field,&grass,&cows,&lions,&c_grass,&c_cows,&c_lions);
        hunger(field,&grass,&cows,&lions,&c_grass,&c_cows,&c_lions);
        decay(&grass,&cows,&lions,&c_grass,&c_cows,&c_lions);
        // usleep(1000);
        fflush(fp);
        if(cows <= 0 || grass <= 0 || lions <= 0) break;

        // 突然の個体増減
        if(day == 4000){
            // cow の数を増やす
            int num = 1000;
            cows += num;
            // 同時にcow種族がもつ炭素数もふやす
            c_cows += num*(UNDER_NUM_COW+UPPER_NUM_COW)/2;
            // フィールドに追加
            for(int i = 0;i < num;i ++){
                int x = rand()%FIELD_WIDTH;
                int y = rand()%FIELD_HEIGHT;
                field[y][x][1] ++; // 2: lion, 1: cow, 0: grass 
            }

            // lion の数を増やす
            // int num = 1000;
            // lions += num;
            // 同時にライオン種族がもつ炭素数もふやす
            // c_lions += num*(UNDER_NUM_LION+UPPER_NUM_LION)/2;
            // フィールドに追加
            // for(int i = 0;i < num;i ++){
                // int x = rand()%FIELD_WIDTH;
                // int y = rand()%FIELD_HEIGHT;
                // field[y][x][2] ++; // 2: lion, 1: cow, 0: grass 
            // }

            // grass の数を減らす 
            // int num = 7000;
            // grass -= num;
            // 同時にgrassが持つ炭素数も増やす
            // c_grass -= num*(UNDER_NUM_GRASS+UPPER_NUM_GRASS)/2;
            // フィールドから削除
            // for(int i = 0;i < num;i ++){
                // kill_in_field(field,0); // 2: lion, 1: cow, 0: grass 
            // }
        }

        // fprintf(f,"%d, %lld, %lld, %lld\n",day,lions,cows,grass);

    }
    print_field(fp,field,day);
    print_pyramid(fp,&grass,&cows,&lions,&c_grass,&c_cows,&c_lions);
    system("clear");
    // fclose(f);
    // fprintf(fp,"\n\nfile closed!\n");
    return 0;
}

void abrt(int sig){
    e_flag = 1;
}
