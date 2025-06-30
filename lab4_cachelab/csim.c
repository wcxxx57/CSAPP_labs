#include "cachelab.h"
#include <getopt.h>//用于解析命令行参数(如optarg)
#include <stdlib.h>
#include <unistd.h>//getopt定义在这里
#include <string.h>
#include <stdio.h>

//定义一些需要用到的全局变量
static int verbose=0;
static int s;
static int S;
static int E;
static int b;
static int hit_count=0;
static int miss_count=0;
static int eviction_count=0;
typedef struct{
    int valid,tag,stamp;//stamp是时间戳 用于记录最近最少使用
}cache_line;
cache_line** cache=NULL;//cache为全局变量
char filename[100];//输入的文件名
void parseCommand(int argc,char* argv[]){
    int opt;
    while((opt=getopt(argc,argv,"vs:E:b:t:"))!=-1){
        switch (opt){
        case 'v':
            verbose=1;
            break;
        case 's':
            s=atoi(optarg);//atoi在stdlib中
            S=1<<s;
            break;
        case 'E':
            E=atoi(optarg);
            break;
        case 'b':
            b=atoi(optarg);
            break;
        case 't':
            strcpy(filename,optarg);
            break;
        default:
            break;
        }
    }
}
void update(unsigned address){
    unsigned set=(address>>b)&((1<<s)-1);//注意set mask的构建
    unsigned tag=address>>(s+b);
    for(int i=0;i<E;i++){
        if(cache[set][i].tag==tag && cache[set][i].valid){
            hit_count++;
            cache[set][i].stamp=0;
            if(verbose) printf("hit ");        
            return;
        }
    }//hit
    miss_count++;//未hit都先miss
    if(verbose) printf("miss ");        
    for(int i=0;i<E;i++){
        if(cache[set][i].valid==0){//存在空line
            cache[set][i].valid=1;
            cache[set][i].tag=tag;
            cache[set][i].stamp=0;
            return;
        }
    }//存在空line miss
    eviction_count++;//没有空的了 只能替换
    if(verbose) printf("eviction ");        
    int max_stamp_index=0;
    for(int i=1;i<E;i++){
        if(cache[set][i].stamp>cache[set][max_stamp_index].stamp){
            max_stamp_index=i;
        }
    }//找到最近最少使用（即搁置最久）
    cache[set][max_stamp_index].tag=tag;
    cache[set][max_stamp_index].stamp=0;
    return;
}
void stamp_update(){
    for(int i=0;i<S;i++){
        for(int j=0;j<E;j++){
            if(cache[i][j].valid){
                cache[i][j].stamp++;
            }
        }
    }
    return;
}
void cacheSimulate(char* filename){
    //动态开辟cache的空间
    cache=(cache_line**)malloc(sizeof(cache_line*)*S);
    for(int i=0;i<S;i++){
        cache[i]=(cache_line*)malloc(sizeof(cache_line)*E);
    }
    for(int i=0;i<S;i++){
        for(int j=0;j<E;j++){
            cache[i][j].valid=0;
            cache[i][j].tag=cache[i][j].stamp=-1;
        }//初始化
    }
    //读文件
    FILE* fp=fopen(filename,"r");
    if(fp==NULL){
        printf("the file is wrong");
        exit(-1);//立即终止当前程序
    }
    char buffer[1000];
    char type;
    unsigned int address;
    int temp;
    while(fgets(buffer,1000,fp)){//fgets成功时返回buffer,文件末尾返回null
        int n=sscanf(buffer," %c %x,%d",&type,&address,&temp);//从字符串中按指定格式提取数据 返回值为成功匹配的变量个数
        if(n<3){
            continue;
        }//处理I的情况
        if(verbose){
            printf("%c %x,%d ",type,address,temp);
        }//verbose模式
        switch (type)
        {
            case 'M':
                update(address);//fall through
            case 'L':
            case 'S':
                update(address);
                break;
        }
        if(verbose) printf("\n");
        stamp_update();//一次操作后时间戳update
    }
    fclose(fp);
    return;
}

int main(int argc,char *argv[]){
    parseCommand(argc,argv);
    cacheSimulate(filename);
    for(int i=0;i<S;i++){
        free(cache[i]);
    }
    free(cache);//释放内存
    printSummary(hit_count,miss_count,eviction_count);
    return 0;
}
