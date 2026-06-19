/***************************************************************************************
 * Copyright (c) 2014-2024 Zihao Yu, Nanjing University
 *
 * NEMU is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 *PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 *KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 *NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * See the Mulan PSL v2 for more details.
 ***************************************************************************************/
#include <isa.h>
#include <memory/vaddr.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
enum
{
  TK_NOTYPE = 256,
  TK_PLUS,
  TK_EQ,
  TK_MINUS,
  TK_MUL,
  TK_DIV,
  TK_NUM,
  TK_LPAREN,
  TK_RPAREN,
  TK_REG,
  TK_DEREF
  /* TODO: Add more token types */
};

static struct rule
{
  const char *regex;
  int token_type;
} rules[] = {

    /* TODO: Add more rules.
     * Pay attention to the precedence level of different rules.
     */

    {" +", TK_NOTYPE}, // spaces
    {"\\+", TK_PLUS},      // plus
    {"==", TK_EQ},     // equal
    {"-", TK_MINUS},   
    {"\\*", TK_MUL},
    {"/", TK_DIV},
    {"0x[0-9a-fA-F]+",TK_NUM},
    {"0b[0-1]+",TK_NUM},
    {"\\$[a-z0-9]+",TK_REG},
    {"[0-9]+",TK_NUM},
    {"\\(",TK_LPAREN},
    {"\\)",TK_RPAREN}
};

#define NR_REGEX ARRLEN(rules)
static word_t parse_expr();
static regex_t re[NR_REGEX] = {};
static bool is_error = false;
/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex()
{
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i++)
  {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0)
    {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token
{
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;
static int token_idx __attribute__((used)) = 0;

static bool make_token(char *e)
{
  int position = 0;
  int i;
  regmatch_t pmatch;
  nr_token = 0;
  while (e[position] != '\0')
  {
    for (i = 0; i < NR_REGEX; i++)
    {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0)
      {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;
        if(rules[i].token_type!=TK_NOTYPE)
        {
          Token *token = &tokens[nr_token++];
          token->type = rules[i].token_type;
          // Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i,
          // rules[i].regex, position, substr_len, substr_len, substr_start);
          if(token->type==TK_NUM || token->type==TK_REG)
          {
            strncpy(token->str,substr_start,substr_len);
            token->str[substr_len] = '\0';
          }
          else{token->str[0] = '\0';}
        }
        position += substr_len;
        switch (rules[i].token_type)
        {
        // default:
        //   TODO();
        }
        break;
      }
    }
    if (i == NR_REGEX)
    {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }
  return true;
}
static word_t parse_factor()
{
  if(tokens[token_idx].type == TK_NUM)
  {
    word_t val = (word_t)strtol(tokens[token_idx++].str,0,0);
    return val;
  }
  if(tokens[token_idx].type == TK_LPAREN)
  {
    token_idx++;
    word_t val = parse_expr();
    if(tokens[token_idx].type!=TK_RPAREN){printf("Syntax Error\n");is_error = true;return 0;}
    token_idx++;
    return val;
  }
  if(tokens[token_idx].type == TK_REG)
  {
    char* regname;
    regname = tokens[token_idx++].str;
    bool success;
    if(*regname=='$'){regname++;}
    word_t val = isa_reg_str2val(regname,&success);
    if(success){return val;}
  }
  if(tokens[token_idx].type == TK_DEREF)
  {
    token_idx++;
    word_t addr = parse_factor();
    word_t val = vaddr_read(addr,4);
    return val;
  }
  printf("Syntax Error\n");
  is_error = true;
  return 0;
}
static word_t parse_term()
{
  word_t val = parse_factor();
  while(!is_error && (tokens[token_idx].type == TK_MUL || tokens[token_idx].type == TK_DIV))
  {
    word_t op = tokens[token_idx++].type;
    word_t next = parse_factor();
    if(op == TK_MUL){val *= next;}
    else if(op == TK_DIV)
    {
      if(!next){printf("Math Error\n");is_error = true;return 0;}
      val /= next;
    }
  }
  return val;
}
static word_t parse_expr()
{
  word_t val = parse_term();
  while(!is_error && (tokens[token_idx].type == TK_PLUS || tokens[token_idx].type == TK_MINUS))
  {
    word_t op = tokens[token_idx++].type;
    word_t next = parse_term();
    if(op == TK_PLUS){val += next;}
    else if(op == TK_MINUS){val -= next;}
  }
  return val;
}
word_t expr(char *e, bool *success)
{
  is_error = false;
  nr_token = 0;
  token_idx = 0;
  memset(tokens, 0, sizeof(tokens));
  for(int i = 0;i<nr_token;i++)
  {
    if(tokens[i].type == TK_MUL)
    {
      if(tokens[i-1].type == TK_NUM || tokens[i-1].type == TK_REG || tokens[i-1].type == TK_RPAREN){}
      else if(i==0){tokens[i].type = TK_DEREF;}
      else{tokens[i].type = TK_DEREF;}
    }
  }
  if (!make_token(e))
  {
    *success = false;
    return 0;
  }
  word_t result = parse_expr();
  if(nr_token!=token_idx || is_error)
  {
    *success = false;
    return 0;
  }
  *success = true;
  return result;
}
