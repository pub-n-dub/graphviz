/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * https://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

#include "tcldot.h"
#include <string.h>
#include <util/streq.h>

static int edgecmd_internal(ClientData clientData, Tcl_Interp *interp, int argc,
                            char *argv[]) {
  char **argv2;
  int i, j, argc2;
  Agraph_t *g;
  Agedge_t *e;
  Agsym_t *a;
  gctx_t *gctx = (gctx_t *)clientData;

  if (argc < 2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                     "\" option ?arg arg ...?", NULL);
    return TCL_ERROR;
  }
  e = cmd2e(argv[0]);
  if (!e) {
    Tcl_AppendResult(interp, "edge \"", argv[0], "\" not found", NULL);
    return TCL_ERROR;
  }
  g = agraphof(agtail(e));

  if (streq("delete", argv[1])) {
    deleteEdge(gctx, g, e);
    return TCL_OK;

  } else if (streq("listattributes", argv[1])) {
    listEdgeAttrs(interp, g);
    return TCL_OK;

  } else if (streq("listnodes", argv[1])) {
    Tcl_AppendElement(interp, obj2cmd(agtail(e)));
    Tcl_AppendElement(interp, obj2cmd(aghead(e)));
    return TCL_OK;

  } else if (streq("queryattributes", argv[1])) {
    for (i = 2; i < argc; i++) {
      if (Tcl_SplitList(interp, argv[i], &argc2, (const char ***)&argv2) !=
          TCL_OK)
        return TCL_ERROR;
      for (j = 0; j < argc2; j++) {
        if ((a = agfindedgeattr(g, argv2[j]))) {
          Tcl_AppendElement(interp, agxget(e, a));
        } else {
          Tcl_AppendResult(interp, "no attribute named \"", argv2[j], "\"",
                           NULL);
          Tcl_Free((char *)argv2);
          return TCL_ERROR;
        }
      }
      Tcl_Free((char *)argv2);
    }
    return TCL_OK;

  } else if (streq("queryattributevalues", argv[1])) {
    for (i = 2; i < argc; i++) {
      if (Tcl_SplitList(interp, argv[i], &argc2, (const char ***)&argv2) !=
          TCL_OK)
        return TCL_ERROR;
      for (j = 0; j < argc2; j++) {
        if ((a = agfindedgeattr(g, argv2[j]))) {
          Tcl_AppendElement(interp, argv2[j]);
          Tcl_AppendElement(interp, agxget(e, a));
        } else {
          Tcl_AppendResult(interp, "no attribute named \"", argv2[j], "\"",
                           NULL);
          Tcl_Free((char *)argv2);
          return TCL_ERROR;
        }
      }
      Tcl_Free((char *)argv2);
    }
    return TCL_OK;

  } else if (streq("setattributes", argv[1])) {
    if (argc == 3) {
      if (Tcl_SplitList(interp, argv[2], &argc2, (const char ***)&argv2) !=
          TCL_OK)
        return TCL_ERROR;
      if ((argc2 == 0) || (argc2 % 2)) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                         "\" setattributes attributename attributevalue "
                         "?attributename attributevalue? ?...?",
                         NULL);
        Tcl_Free((char *)argv2);
        return TCL_ERROR;
      }
      setedgeattributes(agroot(g), e, argv2, argc2);
      Tcl_Free((char *)argv2);
    } else {
      if ((argc < 4) || (argc % 2)) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                         "\" setattributes attributename attributevalue "
                         "?attributename attributevalue? ?...?",
                         NULL);
        return TCL_ERROR;
      }
      setedgeattributes(agroot(g), e, &argv[2], argc - 2);
    }
    return TCL_OK;

  } else if (streq("showname", argv[1])) {
    const char *const s = agisdirected(g) ? "->" : "--";
    Tcl_AppendResult(interp, agnameof(agtail(e)), s, agnameof(aghead(e)), NULL);
    return TCL_OK;

  } else {
    Tcl_AppendResult(interp, "bad option \"", argv[1], "\": must be one of:",
                     "\n\tdelete, listattributes, listnodes,",
                     "\n\tueryattributes, queryattributevalues,",
                     "\n\tsetattributes, showname", NULL);
    return TCL_ERROR;
  }
}

int edgecmd(ClientData clientData, Tcl_Interp *interp, int argc,
            const char *argv[]) {
  char **argv_copy = tcldot_argv_dup(argc, argv);
  int rc = edgecmd_internal(clientData, interp, argc, argv_copy);
  tcldot_argv_free(argc, argv_copy);
  return rc;
}
