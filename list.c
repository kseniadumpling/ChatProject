#include "header.h"


void push(node **head_ref, int new_data) { 
    node *new_node = (node*) malloc(sizeof(node)); 
    new_node->data  = new_data; 
    new_node->next = (*head_ref); 
    (*head_ref)    = new_node; 
} 

void append(node **head_ref, int new_data) { 
    node *new_node = (node*) malloc(sizeof(node)); 
    node *last = *head_ref; 
    new_node->data  = new_data; 
    new_node->next = NULL; 
    if (*head_ref == NULL) { 
       *head_ref = new_node; 
       return; 
    } 
    while (last->next != NULL) 
        last = last->next; 
    last->next = new_node; 
    return; 
} 
  
void print_list(node *node) { 
  while (node != NULL) { 
     printf(" %d ", node->data); 
     node = node->next; 
  } 
} 

void delete_node(node **head_ref, int key) { 
    node *temp = *head_ref, *prev; 
    if (temp != NULL && temp->data == key) { 
        *head_ref = temp->next;  
        free(temp);               
        return; 
    } 
    while (temp != NULL && temp->data != key) { 
        prev = temp; 
        temp = temp->next; 
    } 
    if (temp == NULL) return;  
    prev->next = temp->next; 
    free(temp); 
} 