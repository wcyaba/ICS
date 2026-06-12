/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

static int idx = 0;  // 当前写入位置

// 生成随机数字 (0-99)
static void gen_num() {
    int num = rand() % 100;
    idx += sprintf(buf + idx, "%d", num);
}

// 生成随机运算符
static void gen_rand_op() {
    char op;
    switch (rand() % 4) {
        case 0: op = '+'; break;
        case 1: op = '-'; break;
        case 2: op = '*'; break;
        default: op = '/'; break;
    }
    buf[idx++] = op;
}

// 随机插入空格 (30% 概率)
static void maybe_insert_space() {
    if (rand() % 100 < 30) {
        buf[idx++] = ' ';
    }
}

// 生成随机表达式
static void gen_rand_expr() {
    int choice = rand() % 3;
    
    if (choice == 0) {
        // 数字
        gen_num();
        maybe_insert_space();
    } 
    else if (choice == 1) {
        // 括号表达式
        buf[idx++] = '(';
        maybe_insert_space();
        gen_rand_expr();
        maybe_insert_space();
        buf[idx++] = ')';
        maybe_insert_space();
    } 
    else {
        // 二元运算
        gen_rand_expr();
        maybe_insert_space();
        gen_rand_op();
        maybe_insert_space();
        gen_rand_expr();
    }
}

int main(int argc, char *argv[]) {
    int seed = time(0);
    srand(seed);
    int loop = 1;
    if (argc > 1) {
        sscanf(argv[1], "%d", &loop);
    }
    
    for (int i = 0; i < loop; i++) {
        idx = 0;
        gen_rand_expr();
        buf[idx] = '\0';  // 添加字符串结束符
        
        sprintf(code_buf, code_format, buf);
        
        FILE *fp = fopen("/tmp/.code.c", "w");
        assert(fp != NULL);
        fputs(code_buf, fp);
        fclose(fp);
        
        int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
        if (ret != 0) continue;
        
        fp = popen("/tmp/.expr", "r");
        assert(fp != NULL);
        
        int result;
        ret = fscanf(fp, "%d", &result);
        pclose(fp);
        
        printf("%u %s\n", result, buf);
    }
    
    return 0;
}