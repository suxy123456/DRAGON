#include "adt.h"
#include <string.h>

int adtlist_getcount(struct adtlist* myadtlist)
{
  if (myadtlist == NULL) 
    return 0;

  return myadtlist->count;
}

void adtlist_add(struct adtlist* myadtlist, void* mydata)
{
  struct adtlistnode* mynode;

  if (myadtlist == NULL) {
    myadtlist = malloc(sizeof(struct adtlist));
    memset(myadtlist, 0, sizeof(struct adtlist));
  }

  mynode = malloc(sizeof(struct adtlistnode));

  if (mynode == NULL)
    return;

  mynode->data = mydata;

  if (myadtlist->head == NULL) {
    myadtlist->head = mynode;
    myadtlist->tail = mynode;
    mynode->next = NULL;
  } else {
    myadtlist->tail->next = mynode;
    myadtlist->tail = mynode;
    mynode->next = NULL;
  }

  myadtlist->count++;
}

void adtlist_free(struct adtlist* myadtlist)
{
  struct adtlistnode *curr, *nextnode;

  if (myadtlist == NULL) 
    return;

  for (curr=myadtlist->head, nextnode = NULL; 
       curr; 
       curr = nextnode) {
    nextnode = curr->next;
    if (curr->data != NULL) 
      free(curr->data);
    free(curr);
  }

  free(myadtlist);
}
