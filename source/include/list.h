#ifndef _LINUX_APP_LIST_H
#define _LINUX_APP_LIST_H

#ifdef __KERNEL__
#include <linux/stddef.h>
#else
#include <stddef.h>
#endif

#ifdef  __cplusplus
#define list_inline inline
#else
	#ifdef _WIN32
		#define list_inline __inline
	#else
		#define list_inline inline
	#endif
#endif


/*
 * These are non-NULL pointers that will result in page faults
 * under normal circumstances, used to verify that nobody uses
 * non-initialized list entries.
 */
#define LIST_POISON1	((void *) 0x00100100)
#define LIST_POISON2	((void *) 0x00200200)

#ifndef INIT_LIST_HEAD
/*
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

struct list_head {
	struct list_head *next,
					*prev;
};
typedef struct list_head LISTHEAD, *PLISTHEAD, LISTNODE, *PLISTNODE;

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name)	\
	struct list_head name = { &(name), &(name) }

#define INIT_LIST_HEAD(ptr)	do{ (ptr)->next = (ptr); (ptr)->prev = (ptr); } while (0)

/*
 * Insert a new node between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static list_inline void __list_add(struct list_head *node,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = node;
	node->next = next;
	node->prev = prev;
	prev->next = node;
}

/**
 * list_add - add a new node
 * @node: new node to be added
 * @head: list head to add it after
 *
 * Insert a new node after the specified head.
 * This is good for implementing stacks.
 */
static list_inline void list_add(struct list_head *node, struct list_head *head)
{
	__list_add(node, head, head->next);
}

/**
 * list_add_tail - add a new node
 * @node: new node to be added
 * @head: list head to add it before
 *
 * Insert a new node before the specified head.
 * This is useful for implementing queues.
 */
static list_inline void list_add_tail(struct list_head *node, struct list_head *head)
{
	__list_add(node, head->prev, head);
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
#define	list_empty(head)		((head)->next == (head))

#define	list_empty_safe(head)	((head)->next == (head) || (head)->next == NULL)

/**
 * list_empty_careful - tests whether a list is
 * empty _and_ checks that no other CPU might be
 * in the process of still modifying either member
 *
 * NOTE: using list_empty_careful() without synchronization
 * can only be safe if the only activity that can happen
 * to the list node is list_del_init(). Eg. it cannot be used
 * if another CPU could re-list_add() it.
 *
 * @head: the list to test.
 */
static list_inline int list_empty_careful(const struct list_head *head)
{
	struct list_head *next = head->next;
	return (next == head) && (next == head->prev);
}

/*
 * Delete a list node by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static list_inline void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

/**
 * list_del - deletes node from list.
 * @node: the element to delete from the list.
 * Note: list_empty on node does not return true after this, the node is
 * in an undefined state.
 */
static list_inline void list_del(struct list_head *node)
{
	__list_del(node->prev, node->next);
#if 0
	node->next = LIST_POISON1;
	node->prev = LIST_POISON2;
#endif
}

/**
 * list_del_init - deletes node from list and reinitialize it.
 * @node: the element to delete from the list.
 */
static list_inline void list_del_init(struct list_head *node)
{
	__list_del(node->prev, node->next);
	INIT_LIST_HEAD(node);
}

/**
 * list_move - delete from one list and add as another's head
 * @node: the node to move
 * @head: the head that will precede our node
 */
static list_inline void list_move(struct list_head *node, struct list_head *head)
{
        __list_del(node->prev, node->next);
        list_add(node, head);
}

/**
 * list_move_tail - delete from one list and add as another's tail
 * @node: the node to move
 * @head: the head that will follow our node
 */
static list_inline void list_move_tail(struct list_head *list,
				  struct list_head *head)
{
        __list_del(list->prev, list->next);
        list_add_tail(list, head);
}

static list_inline void __list_splice(struct list_head *list,
				 struct list_head *head)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;
	struct list_head *at = head->next;

	first->prev = head;
	head->next = first;

	last->next = at;
	at->prev = last;
}

/**
 * list_splice - join two lists
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */
static list_inline void list_splice(struct list_head *list, struct list_head *head)
{
	if (!list_empty(list))
		__list_splice(list, head);
}

/**
 * list_splice_init - join two lists and reinitialise the emptied list.
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 *
 * The list at @list is reinitialised
 */
static list_inline void list_splice_init(struct list_head *list,
				    struct list_head *head)
{
	if (!list_empty(list)) {
		__list_splice(list, head);
		INIT_LIST_HEAD(list);
	}
}

/**
 * list_entry - get the struct for this node
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member)  ((type *)((char *)ptr - offsetof(type,member)))

/**
 * list_get_init - prepare a pos for use as a start point in list_next
 * @pos		the &struct list_head to use as current position.
 * @head:	the head for your list.
 */
#define	list_get_init(pos, head)	(pos = (head))
/**
 * list_next - get the next node after current pos for the list
 * @pos		the &struct list_head to use as current position.
 * @head:	the head for your list.
 */
#define	list_get_next(pos, head)	(((pos)->next == (head)) ? NULL : (pos)->next)
#define	list_get_prev(pos, head)	(((pos)->prev == (head)) ? NULL : (pos)->prev)
#define	list_get_head(head)		(list_empty(head) ? NULL : (head)->next)
#define	list_get_tail(head)		(list_empty(head) ? NULL : (head)->prev)

/**
 * list_for_each	-	iterate over a list
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head)	\
	for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * list_for_each_prev	-	iterate over a list backwards
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_for_each_prev(pos, head)	\
	for (pos = (head)->prev; pos != (head); pos = pos->prev)

/**
 * list_for_each_safe	-	iterate over a list safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop counter.
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

/**
 * list_for_each_entry	-	iterate over list of given type
 * @tpos:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry(tpos, head, type, member)				\
	for	(tpos = list_entry((head)->next, type, member);	\
		&tpos->member != (head);	\
		tpos = list_entry(tpos->member.next, type, member))

/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @tpos:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry_reverse(tpos, head, type, member)			\
	for	(tpos = list_entry((head)->prev, type, member);	\
		&tpos->member != (head);	\
		tpos = list_entry(tpos->member.prev, type, member))

/**
 * list_prepare_entry - prepare a pos entry for use as a start point in
 *			list_for_each_entry_continue
 * @tpos:	the type * to use as a start point
 * @head:	the head of the list
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_prepare_entry(tpos, head, type, member)	\
	((tpos) ? list_entry(tpos->member.next, type, member) :	\
			list_entry(head, type, member))

/**
 * list_for_each_entry_continue -	iterate over list of given type
 *			continuing after existing point
 * @tpos:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry_continue(tpos, head, type, member)	\
	for (tpos = list_entry(tpos->member.next, type, member);	\
		&tpos->member != (head);	\
		tpos = list_entry(tpos->member.next, type, member))
/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @tpos:	the type * to use as a loop counter.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry_safe(tpos, n, head, type, member)	\
	for (tpos = list_entry((head)->next, type, member),	\
		n = list_entry(tpos->member.next, type, member);	\
		&tpos->member != (head);	\
		tpos = n, n = list_entry(n->member.next, type, member))

/**
 * list_count - get total entrys number
 * @head: the head of the list
 */
static list_inline unsigned long list_count(const struct list_head *head)
{
	const struct list_head *p;
	unsigned long cnt = 0;

	list_for_each(p, head)
		cnt++;
	return cnt;
}
#endif /* INIT_LIST_HEAD */


#ifndef INIT_HLIST_HEAD
/*
 * Double linked lists with a single pointer list head.
 * Mostly useful for hash tables where the two pointer list head is
 * too wasteful.
 * You lose the ability to access the tail in O(1).
 */

struct hlist_head {
	struct hlist_node *first;
};

struct hlist_node {
	struct hlist_node *next, **pprev;
};

typedef struct hlist_head HLISTHEAD, *PHLISTHEAD;
typedef struct hlist_node HLISTNODE, *PHLISTNODE;

#define HLIST_HEAD_INIT		{ NULL }
#define HLIST_HEAD(name)	struct hlist_head name = { NULL }
#define INIT_HLIST_HEAD(ptr)		((ptr)->first = NULL)
#define INIT_HLIST_NODE(ptr)		((ptr)->next = NULL, (ptr)->pprev = NULL)

static list_inline int hlist_unhashed(const struct hlist_node *n)
{
	return !n->pprev;
}

static list_inline int hlist_empty(const struct hlist_head *h)
{
	return !h->first;
}

static list_inline void __hlist_del(struct hlist_node *n)
{
	struct hlist_node *next = n->next;
	struct hlist_node **pprev = n->pprev;
	*pprev = next;
	if (next)
		next->pprev = pprev;
}

static list_inline void hlist_del(struct hlist_node *n)
{
	__hlist_del(n);
	n->next = (struct hlist_node *)LIST_POISON1;
	n->pprev = (struct hlist_node **)LIST_POISON2;
}

static list_inline void hlist_del_init(struct hlist_node *n)
{
	if (n->pprev)  {
		__hlist_del(n);
		INIT_HLIST_NODE(n);
	}
}

static list_inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
	struct hlist_node *first = h->first;
	n->next = first;
	if (first)
		first->pprev = &n->next;
	h->first = n;
	n->pprev = &h->first;
}

/* next must be != NULL */
static list_inline void hlist_add_before(struct hlist_node *n,
					struct hlist_node *next)
{
	n->pprev = next->pprev;
	n->next = next;
	next->pprev = &n->next;
	*(n->pprev) = n;
}

static list_inline void hlist_add_after(struct hlist_node *n,
					struct hlist_node *next)
{
	next->next = n->next;
	n->next = next;
	next->pprev = &n->next;

	if(next->next)
		next->next->pprev  = &next->next;
}

#define hlist_entry(ptr, type, member)  ((type *)((char *)ptr - offsetof(type,member)))

#define hlist_for_each(pos, head) \
	for (pos = (head)->first; pos; pos = pos->next)

#define hlist_for_each_safe(pos, n, head) \
	for (pos = (head)->first; pos && (n = pos->next, 1); pos = n)


/**
 * hlist_for_each_entry	- iterate over list of given type
 * @tpos:	the type * to use as a loop counter.
 * @pos:	the &struct hlist_node to use as a loop counter.
 * @head:	the head for your list.
 * @member:	the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry(tpos, pos, head, type, member)	\
	for (pos = (head)->first;	\
		pos && (tpos = hlist_entry(pos, type, member), 1); \
		pos = pos->next)

/**
 * hlist_for_each_entry_continue - iterate over a hlist continuing after existing point
 * @tpos:	the type * to use as a loop counter.
 * @pos:	the &struct hlist_node to use as a loop counter.
 * @member:	the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_continue(tpos, pos, type, member)	\
	for (pos = (pos)->next;	\
		pos && (tpos = hlist_entry(pos, type, member), 1);	\
		pos = pos->next)

/**
 * hlist_for_each_entry_from - iterate over a hlist continuing from existing point
 * @tpos:	the type * to use as a loop counter.
 * @pos:	the &struct hlist_node to use as a loop counter.
 * @member:	the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_from(tpos, pos, type, member)	\
	for (; pos &&	 (tpos = hlist_entry(pos, type, member), 1);	\
		pos = pos->next)

/**
 * hlist_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @tpos:	the type * to use as a loop counter.
 * @pos:	the &struct hlist_node to use as a loop counter.
 * @n:		another &struct hlist_node to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_safe(tpos, pos, n, head, type, member)	\
	for (pos = (head)->first;	\
		pos && (n = pos->next, 1) &&	\
		(tpos = hlist_entry(pos, type, member), 1);	\
		pos = n)

/**
 * hlist_count - get total entrys number
 * @head: the head of the list
 */
static list_inline unsigned long hlist_count(const struct hlist_head *head)
{
	const struct hlist_node *p;
	unsigned long cnt = 0;

	hlist_for_each(p, head)
		cnt++;
	return cnt;
}
#endif /* INIT_HLIST_HEAD */


#endif /* _LIST_H_ */

