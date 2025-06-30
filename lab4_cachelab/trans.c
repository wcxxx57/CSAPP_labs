/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]){
    int a,b,c,d,e,f,g,h;//8个局部变量
    if(M==32){
        for(int n=0;n<N;n+=8){
            for(int m=0;m<M;m+=8){//分成8*8的小块
                for(int i=n;i<n+8;i++){
                    a=A[i][m];
                    b=A[i][m+1];
                    c=A[i][m+2];
                    d=A[i][m+3];
                    e=A[i][m+4];
                    f=A[i][m+5];
                    g=A[i][m+6];
                    h=A[i][m+7];
    
                    B[m][i]=a;
                    B[m+1][i]=b;
                    B[m+2][i]=c;
                    B[m+3][i]=d;
                    B[m+4][i]=e;
                    B[m+5][i]=f;
                    B[m+6][i]=g;
                    B[m+7][i]=h;
                }
            }
        }
    }
    if(M==64){
        int k;
        for(int m=0;m<64;m+=8){//行
            for(int n=0;n<64;n+=8){//列
                for(k=0;k<4;k++){
                    a=A[m+k][n];
                    b=A[m+k][n+1];
                    c=A[m+k][n+2];
                    d=A[m+k][n+3];
                    e=A[m+k][n+4];
                    f=A[m+k][n+5];
                    g=A[m+k][n+6];
                    h=A[m+k][n+7];//从A中取前四行(循环下来共会有4次miss)

                    B[n][m+k]=a;
                    B[n+1][m+k]=b;
                    B[n+2][m+k]=c;
                    B[n+3][m+k]=d;
                    B[n][m+k+4]=e;
                    B[n+1][m+k+4]=f;
                    B[n+2][m+k+4]=g;//!注意A的行号是B的列号！写矩阵索引的时候注意！
                    B[n+3][m+k+4]=h;//转置后读到B的上半部分(共4次miss)
                }
                for(k=0;k<4;k++){
                    a=A[m+4][n+k];
                    b=A[m+5][n+k];
                    c=A[m+6][n+k];
                    d=A[m+7][n+k];//存A
                    e=B[n+k][m+4];
                    f=B[n+k][m+5];
                    g=B[n+k][m+6];
                    h=B[n+k][m+7];//存B

                    B[n+k][m+4]=a;
                    B[n+k][m+5]=b;
                    B[n+k][m+6]=c;
                    B[n+k][m+7]=d;//读完后写 减少miss
                    B[n+k+4][m]=e;
                    B[n+k+4][m+1]=f;
                    B[n+k+4][m+2]=g;
                    B[n+k+4][m+3]=h;
                }//把A的左下角转置到B的右上角 同时把B右上角复制到B的左下角
                for(k=4;k<8;k++){
                    a=A[m+k][n+4];
                    b=A[m+k][n+5];
                    c=A[m+k][n+6];
                    d=A[m+k][n+7];
                    B[n+k][m+4]=a;
                    B[n+k][m+5]=b;
                    B[n+k][m+6]=c;
                    B[n+k][m+7]=d;
                }//把A的右下角先复制到B的右下角
                for(k=4;k<8;k++){
                    for(int i=k+1;i<8;i++){
                        a=B[n+k][m+i];
                        B[n+k][m+i]=B[n+i][m+k];//!
                        B[n+i][m+k]=a;
                    }
                }//在B处进行转置 防止出现多余的miss
            }
        }
    }
    
    if(M==61){
        for(int n=0;n<N;n+=17){
            for(int m=0;m<M;m+=17){//分成17*17的小块
                for(int i=n;i<n+17&&i<N;i++){
                    for(int j=m;j<m+17&&j<M;j++){
                        B[j][i]=A[i][j];
                    }
                }
            }
        }
    }
}


/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}
char trans_88_desc[]="8*8 blocking";
void trans_88(int M,int N,int A[N][M],int B[M][N]){
    for(int n=0;n<N;n+=8){
        for(int m=0;m<M;m+=8){//分成8*8的小块
            for(int i=n;i<n+8;i++){
                for(int j=m;j<m+8;j++){
                    B[j][i]=A[i][j];
                }
            }
        }
    }
}
char trans_copy_desc[]="8*8 (copy&tranc)";
void trans_copy(int M,int N,int A[N][M],int B[M][N]){
    for (int i = 0; i < N; i += 8) { // 当前行
        for (int j = 0; j < M; j += 8) { // 当前列
            int a,b,c,d,e,f,g,h;
            for (int k = 0;k < 8;k++) {
                a = A[i + k][j];
                b = A[i + k][j + 1];
                c = A[i + k][j + 2];
                d = A[i + k][j + 3];
                e = A[i + k][j + 4];
                f = A[i + k][j + 5];
                g = A[i + k][j + 6];
                h = A[i + k][j + 7];

                B[j + k][i] = a;
                B[j + k][i + 1] = b;
                B[j + k][i + 2] = c;
                B[j + k][i + 3] = d;
                B[j + k][i + 4] = e;
                B[j + k][i + 5] = f;
                B[j + k][i + 6] = g;
                B[j + k][i + 7] = h;
            }//将A复制到B的对应位置
            for (int k = 0;k < 8;++k) {
                // 对角线不用交换
                for (int l = 0;l < k;++l) {
                    a = B[j + k][i + l];
                    B[j + k][i + l] = B[j + l][i + k];
                    B[j + l][i + k] = a;
                }
            }//转置B
        }
    }
}
char trans_64_desc[]="simply 4*4 blocking for 64*64";
void trans_64(int M,int N,int A[N][M],int B[M][N]){
    for(int n=0;n<N;n+=4){
        for(int m=0;m<M;m+=4){//分成4*4的小块
            for(int i=n;i<n+4;i++){
                for(int j=m;j<m+4;j++){
                    B[j][i]=A[i][j];
                }
            }
        }
    }
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 
    registerTransFunction(trans_88, trans_88_desc); 
    registerTransFunction(trans_copy, trans_copy_desc); 
    registerTransFunction(trans_64, trans_64_desc); 





}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

