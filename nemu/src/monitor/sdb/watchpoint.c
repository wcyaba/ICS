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

#include "sdb.h"
#define NR_WP 32

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;
  char expr[32];
  word_t value;
  /* TODO: Add more members if necessary */
} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }
  head = NULL;
  free_ = wp_pool;
}
void wp_add(char *args)
{
  if(!free_){printf("No free point\n");return;}
  WP *wp;
  wp = free_;
  free_ = free_->next;
  wp->next = free_;
  bool success;
  word_t val = expr(args,&success);
  if(!success)
  {
    printf("Invalid expression\n");
    wp->next = free_;
    free_ = wp;
    return;
  }
  strncpy(wp->expr,args,31);
  wp->expr[31] = '\0';
  wp->value = val;
  wp->next = head;
  head = wp;
}
void wp_check()
{
  bool success;
  WP *Temp = head;
  while(Temp)
  {
    word_t new_value = expr(Temp->expr,&success);
    if(!success){printf("Watch Point Error!\n");return;}
    if(new_value != Temp->value)
    {
      printf("Watch Point %d: %s\n",Temp->NO,Temp->expr);
      printf("Old value = %d\n",Temp->value);
      Temp->value = new_value;
      printf("New value = %d\n",Temp->value);
      nemu_state.state = NEMU_STOP;
      return;
    }
    Temp = Temp->next;
  }
}
void wp_display()
{
  WP *Temp;
  Temp = head;
  while(Temp)
  {
    printf("Watch Point %d: %s\n",Temp->NO,Temp->expr);
    printf("value = %d\n",Temp->value);
    Temp = Temp->next;
  }
  return;
}

/* TODO: Implement the functionality of watchpoint */

