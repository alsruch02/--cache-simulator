#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>


/* 파일 이름 */
#define filename "swim.trace"

struct i_cache {	/* 인스트럭션 캐시 구조체 */
    int tag;
    int valid;
    int time;
};

struct d_cache { /* 데이터 캐시 구조체 */
    int tag;	/* 태그 */
    int valid;	/* valid bit */
    int time;	/* LRU를 구현하기 위한 마지막 접근 시간*/
    int dirty;	/* write back 시 dirty bit */
};

/* 전역 변수 */
int load_hit, load_miss, stor_hit, stor_miss;  //   l hit, miss와 s hit, miss
int d_total, d_miss, d_write;   /* 데이터 캐시 접근 횟수 및 miss 횟수, 메모리 쓰기 횟수 */
int time_count; /* LRU를 구현하기 위한 시간 */

struct i_cache* ip; /* 인스트럭션 캐시를 가리키는 포인터 */
struct d_cache* dp; /* 데이터 캐시를 가리키는 포인터 */

/* 함수 목록 */
void simulation(int c_size, int b_size, int assoc);
void read_data(long long int addr, int c_size, int b_size, int assoc);
void write_data(long long int addr, int c_size, int b_size, int assoc);
int evict(int set, int assoc, char mode);

int main() { 
    int i, j, k;  /* 반복문 인덱스 */
    int input = 0;

    while (input != 2) {
        printf("1. print\n2.exit\n: ");
        scanf("%d", &input);
        /* 전체 시뮬레이션 */
        if (input == 1) {
            printf("cache size | block size | associative | d-miss rate |  mem write\n");
            printf("cache size : 131072\n");
            printf("block size : 128\n");
            printf("associative : 4\n");

            simulation(131072, 128, 4);
            printf("loadhit|loadmis|storhit|stormis|d_total|d_miss_|d_write\n");
            printf("%7d|%7d|%7d|%7d|%7d|%7d|%7d\n", load_hit, load_miss, stor_hit, stor_miss,d_total,d_miss,d_write);
            
        }
    }
    return 0;
}

/* 시뮬레이션 함수: 캐시사이즈와 블럭사이즈, associative를 인자로 받는다. */
void simulation(int c_size, int b_size, int assoc) {
    /* 전역변수 값 초기화 */
    load_hit = load_miss = stor_hit = stor_miss = 0;
    d_total = d_miss = d_write = 0;

    long long int addr; /* 파일로부터 읽은 값을 저장하는 변수 */
    char mode;
    char tmp[12];
    int iTmp;
    int num = c_size / b_size;    //각 캐시의 크기
    double i_res, d_res;    /* miss rate을 저장하는 변수 */
    FILE* fp = NULL;

    ip = (struct i_cache*)calloc(num, sizeof(struct i_cache));
    dp = (struct d_cache*)calloc(num, sizeof(struct d_cache));

    fp = fopen(filename, "r");
    /* 파일로부터 mode와 주소를 읽어 각 함수에 전달한다. */
    while (!feof(fp)) {
        fscanf(fp, "%c %s %d\n", &mode, tmp, &iTmp);
        addr = strtoul(tmp, NULL, 16);

        printf("%c | %8x | %d\n", mode, addr, iTmp);
        switch (mode) {
        case 'l':
            read_data(addr, c_size, b_size, assoc);
            d_total++;	/* 데이터 캐시 접근횟수를 늘린다. */
            break;
        case 's':
            write_data(addr, c_size, b_size, assoc);
            d_total++;	/* 데이터 캐시 접근횟수를 늘린다. */
            break;
        
        }
        time_count++;	/* 하나의 명령을 수행할 때마다 시간을 늘린다. */
    }

    free(ip);
    free(dp);
    d_res = (double)d_miss / (double)d_total;

    printf("%10d   %10d    %10d        %.4lf  %10d\n", c_size, b_size, assoc, d_res, d_write);
    fclose(fp);
}

void read_data(long long int addr, int c_size, int b_size, int assoc) {
    int num_of_sets, set;   /* set의 개수와 입력받은 주소의 set을 저장하는 변수 */
    int i, ev = 0, avail = 10;  /* 반복문의 인덱스와 victim의 인덱스, 그리고 새로 넣을 블럭의 인덱스 */
    struct d_cache* p;  /* 캐시를 가리키는 포인터 */

    num_of_sets = c_size / (b_size * assoc);    /* set의 개수를 구한다. */
    set = (addr / b_size) % num_of_sets;  /* 입력받은 인자로부터 해당 주소의 set을 구한다. */

    /* 캐시에서 해당 set을 검색하여 HIT/MISS를 결정 */
    for (i = 0; i < assoc; i++) {
        p = &dp[set * assoc + i];
        /* valid bit이 1이고 tag값이 일치하면 접근 시간을 바꾸고 HIT */
        if (p->valid == 1 && p->tag == (addr / b_size) / num_of_sets) {
            p->time = time_count;
            load_hit++;
            return;
        }
        /* 새로운 블럭이 들어갈 인덱스 */
        else if (p->valid == 0) {
            if (i < avail)
                avail = i;
        }
    }
    /* set에 해당되는 블럭이 없으므로 MISS이고 새로운 블럭을 올린다. */
    d_miss++;
    load_miss++;
    /* 캐시의 set이 가득찬 경우 */
    if (avail == 10) {
        ev = evict(set, assoc, 'd');
        p = &dp[set * assoc + ev];

        /* victim 블럭의 dirty bit이 1이면 메모리 쓰기를 한다. */
        if (p->dirty)
            d_write++;

        p->valid = 1;
        p->time = time_count;
        p->tag = (addr / b_size) / num_of_sets;
        p->dirty = 0;   //새로 올린 블럭이므로 dirty bit은 0이다.

    }
    /* 캐시의 set에 자리가 있는 경우 */
    else {
        p = &dp[set * assoc + avail];

        p->valid = 1;
        p->time = time_count;
        p->tag = (addr / b_size) / num_of_sets;
        p->dirty = 0;
    }
}

void write_data(long long int addr, int c_size, int b_size, int assoc) {
    int num_of_sets, set;   /* set의 개수와 입력받은 주소의 set을 저장하는 변수 */
    int i, ev = 0, avail = 10;  /* 반복문의 인덱스와 victim의 인덱스, 그리고 새로 넣을 블럭의 인덱스 */
    struct d_cache* p;  /* 캐시를 가리키는 포인터 */

    num_of_sets = c_size / (b_size * assoc);    /* set의 개수를 구한다. */
    set = (addr / b_size) % num_of_sets;  /* 입력받은 인자로부터 해당 주소의 set을 구한다. */

    /* 캐시에서 해당 set을 검색하여 HIT/MISS를 결정 */
    for (i = 0; i < assoc; i++) {
        p = &dp[set * assoc + i];
        /* valid bit이 1이고 tag값이 일치하면 접근 시간을 바꾸고, dirty bit을 1로 변경하고 HIT */
        if (p->valid == 1 && p->tag == (addr / b_size) / num_of_sets) {
            p->time = time_count;
            p->dirty = 1;
            stor_hit++;
            return;
        }
        else if (p->valid == 0) {
            if (i < avail)
                avail = i;
        }
    }
    /* set에 해당되는 블럭이 없으므로 MISS이고 새로운 블럭을 올린다. */
    d_miss++;
    stor_miss++;

    /* 캐시의 set이 가득찬 경우 */
    if (avail == 10) {
        ev = evict(set, assoc, 'd');
        p = &dp[set * assoc + ev];

        /* victim 블럭의 dirty bit이 1이면 메모리 쓰기를 한다. */
        if (p->dirty)
            d_write++;

        p->valid = 1;
        p->time = time_count;
        p->tag = (addr / b_size) / num_of_sets;
        p->dirty = 1;   /* 새로 올린 블럭도 수정했으므로 dirty bit은 1 */
    }
    /* 캐시의 set에 자리가 있는 경우 */
    else {
        p = &dp[set * assoc + avail];
        p->valid = 1;
        p->time = time_count;
        p->tag = (addr / b_size) / num_of_sets;
        p->dirty = 1;
    }
}

/* LRU기법에 따라서 victim을 정하는 함수 */
int evict(int set, int assoc, char mode) {
    int i, tmp_time = 0;  /* 반복문의 인덱스와 시간을 저장하는 변수 */
    int min = time_count + 1, min_i = 0;  /* 최소값을 찾기 위한 변수와 인덱스 변수 */

    /* set에서 time값이 가장 작은 블럭의 인덱스를 찾아 return한다. */
    for (i = 0; i < assoc; i++) {
        if (mode == 'd')
            tmp_time = dp[set * assoc + i].time;
        if (mode == 'i')
            tmp_time = ip[set * assoc + i].time;
        if (min > tmp_time) {
            min = tmp_time;
            min_i = i;
        }
    }
    return min_i;
}
