/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * https://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/


/*
 * grid.c
 * Written by Emden R. Gansner
 *
 * Support for grid to speed up layout. On each pass, nodes are
 * put into grid cells. Given a node, repulsion is only computed 
 * for nodes in one of that nodes 9 adjacent grids.
 */

/* uses PRIVATE interface for NOTUSED */
#define FDP_PRIVATE 1

#include <fdpgen/fdp.h>
#include <fdpgen/grid.h>
#include <common/macros.h>
#include <stddef.h>
#include <string.h>
#include <util/alloc.h>

  /* structure for maintaining a free list of cells */
typedef struct _block {
    cell *mem;			/* block of cells */
    cell *cur;			/* next available cell */
    cell *endp;			/* after last cell */
    struct _block *next;	/* next memory block */
} block_t;

/* newBlock:
 * Create new block of size cells
 */
static block_t *newBlock(int size)
{
    block_t *newb = gv_alloc(sizeof(block_t));
    newb->next = 0;
    newb->mem = gv_calloc(size, sizeof(cell));
    newb->endp = newb->mem + size;
    newb->cur = newb->mem;

    return newb;
}

/* freeBlock:
 * Free malloc'ed memory and block.
 * Recurse to next block
 */
static void freeBlock(block_t * b)
{
    if (b) {
	block_t *next = b->next;
	free(b->mem);
	free(b);
	freeBlock(next);
    }
}

struct _grid {
    Dt_t *data;			/* cells indexed by (i,j) */
    block_t *cellMem;		/* list of memory blocks for cells */
    block_t *cellCur;		/* current block */
    int listSize;		/* memory of nodes */
    node_list *listMem;		/* list of memory for node items */
    node_list *listCur;		/* next node item */
};

/* getCell:
 * Create a new cell using memory blocks.
 */
static cell *getCell(Grid * g)
{
    cell *cp;
    block_t *bp = g->cellCur;	/* current block */

    if (bp->cur == bp->endp) {	/* current block is full */
	if (bp->next == 0) {
	    bp->next = newBlock(2 * (bp->endp - bp->mem));
	}
	bp = g->cellCur = bp->next;
	bp->cur = bp->mem;
    }
    cp = bp->cur++;
    return cp;
}

static int ijcmpf(void *point1, void *point2) {
    const gridpt *p1 = point1;
    const gridpt *p2 = point2;
    if (p1->i < p2->i) {
        return -1;
    }
    if (p1->i > p2->i) {
        return 1;
    }
    if (p1->j < p2->j) {
        return -1;
    }
    if (p1->j > p2->j) {
        return 1;
    }
    return 0;
}

static Grid _grid; // hack because can't attach info. to Dt_t

/* newCell:
 * Allocate a new cell from free store and initialize its indices
 * This is used by the grid discipline to create cells.
 */
static void *newCell(void *obj, Dtdisc_t *disc) {
    cell *cellp = obj;
    cell *newp;

    (void)disc;
    newp = getCell(&_grid);
    newp->p.i = cellp->p.i;
    newp->p.j = cellp->p.j;
    newp->nodes = 0;

    return newp;
}

/* newNode:
 * Allocate a new node item from free store. 
 * Set node value and hook into list.
 * A grid assumes the memory allocated in adjustGrid
 * will be enough more all nodes added.
 */
static node_list *newNode(Grid * g, Agnode_t * n, node_list * nxt)
{
    node_list *newp;

    newp = g->listCur++;
    newp->node = n;
    newp->next = nxt;

    return newp;
}

static Dtdisc_t gridDisc = {
    offsetof(cell, p),
    sizeof(gridpt),
    offsetof(cell, link),
    newCell,
    NULL,
    ijcmpf,
};

/* mkGrid:
 * Create grid data structure.
 * cellHint provides rough idea of how many cells
 * may be needed.
 */
Grid *mkGrid(int cellHint)
{
    Grid *g = &_grid;
    memset(g, 0, sizeof(*g)); // see comment above
    g->data = dtopen(&gridDisc, Dtoset);
    g->cellMem = newBlock(cellHint);
    return g;
}

/* adjustGrid:
 * Set up node list for grid. Make sure the list
 * can handle nnodes nodes.
 * It is assumed no more than nnodes will be added
 * to the grid.
 */
void adjustGrid(Grid * g, int nnodes)
{
    int nsize;

    if (nnodes > g->listSize) {
	nsize = MAX(nnodes, 2 * g->listSize);
	if (g->listMem)
	    free(g->listMem);
	g->listMem = gv_calloc(nsize, sizeof(node_list));
	g->listSize = nsize;
    }
}

/* clearGrid:
 * Reset grid. This clears the dictionary,
 * and reuses available memory.
 */
void clearGrid(Grid * g)
{
    dtclear(g->data);
    g->listCur = g->listMem;
    g->cellCur = g->cellMem;
    g->cellCur->cur = g->cellCur->mem;
}

/* delGrid:
 * Close and free all grid resources.
 */
void delGrid(Grid * g)
{
    dtclose(g->data);
    freeBlock(g->cellMem);
    free(g->listMem);
}

/* addGrid:
 * Add node n to cell (i,j) in grid g.
 */
void addGrid(Grid * g, int i, int j, Agnode_t * n)
{
    cell *cellp;
    cell key;

    key.p.i = i;
    key.p.j = j;
    cellp = dtinsert(g->data, &key);
    cellp->nodes = newNode(g, n, cellp->nodes);
    if (Verbose >= 3) {
	fprintf(stderr, "grid(%d,%d): %s\n", i, j, agnameof(n));
    }
}

typedef int (*walkfn_t)(void*, void*);

/* walkGrid:
 * Apply function walkf to each cell in the grid.
 * The second argument to walkf is the cell; the
 * third argument is the grid. (The first argument
 * is the dictionary.) walkf must return 0.
 */
void walkGrid(Grid *g, int (*walkf)(cell*, Grid*))
{
    dtwalk(g->data, (walkfn_t) walkf, g);
}

/* findGrid;
 * Return the cell, if any, corresponding to
 * indices i,j
 */
cell *findGrid(Grid * g, int i, int j)
{
    cell key;

    key.p.i = i;
    key.p.j = j;
    return dtsearch(g->data, &key);
}

/* gLength:
 * Return the number of nodes in a cell.
 */
int gLength(cell * p)
{
    int len = 0;
    node_list *nodes = p->nodes;

    while (nodes) {
	len++;
	nodes = nodes->next;
    }
    return len;
}
