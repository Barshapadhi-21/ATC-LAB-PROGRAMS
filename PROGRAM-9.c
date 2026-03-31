/*
 * ============================================================
 *   DAG-Based Optimization of a Basic Block
 * ============================================================
 *  Optimizations performed:
 *   1. Common Sub-expression Elimination (CSE)
 *   2. Dead Code Elimination
 *   3. Constant Folding
 *   4. Copy Propagation
 *
 *  Input  : Three-Address Code (TAC) instructions
 *  Output : Optimized TAC regenerated from the DAG
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* ──────────────────────────── Constants ──────────────────────────────── */
#define MAX_NODES      256
#define MAX_INSTRS     256
#define MAX_LABEL_LEN   64
#define MAX_VARS       128

/* ──────────────────────────── Structures ─────────────────────────────── */

/* Operators for DAG nodes */
typedef enum {
    OP_NONE,   /* leaf / value node */
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_AND, OP_OR,  OP_NOT,
    OP_EQ,  OP_NEQ, OP_LT,  OP_GT,  OP_LE,  OP_GE,
    OP_NEG,         /* unary minus */
    OP_ASSIGN       /* copy node */
} Operator;

static const char *op_str[] = {
    "", "+", "-", "*", "/", "%",
    "&&", "||", "!",
    "==", "!=", "<", ">", "<=", ">=",
    "-", "="
};

/* A single DAG node */
typedef struct Node {
    int      id;
    Operator op;
    int      left;          /* index of left child  (-1 = none) */
    int      right;         /* index of right child (-1 = none) */

    /* Leaf / constant info */
    int      is_const;
    long     const_val;
    char     leaf_name[MAX_LABEL_LEN]; /* original var name if leaf */

    /* All variable names attached to this node (result labels) */
    char     labels[MAX_VARS][MAX_LABEL_LEN];
    int      nlabels;

    int      is_dead;       /* marked during dead-code elimination */
} Node;

/* Three-address instruction */
typedef struct {
    char   result[MAX_LABEL_LEN];
    char   op_sym[8];
    char   arg1[MAX_LABEL_LEN];
    char   arg2[MAX_LABEL_LEN];
    int    is_unary;   /* only arg1 is used */
    int    is_copy;    /* result = arg1       */
} Instr;

/* ──────────────────────────── Global State ───────────────────────────── */

static Node   dag[MAX_NODES];
static int    ndag = 0;

/* Maps variable name → latest DAG node that holds its value */
typedef struct { char name[MAX_LABEL_LEN]; int node_id; } VarMap;
static VarMap var_map[MAX_VARS];
static int    nvar_map = 0;

static Instr  instrs[MAX_INSTRS];
static int    ninstrs = 0;

/* Generated (optimized) output instructions */
static char   out_buf[MAX_INSTRS][256];
static int    nout = 0;

/* ──────────────────────────── Helpers ────────────────────────────────── */

static void print_separator(char c, int n) {
    for (int i = 0; i < n; i++) putchar(c);
    putchar('\n');
}

/* ── Operator parsing ── */
static Operator parse_op(const char *s) {
    if (!strcmp(s,"+"))  return OP_ADD;
    if (!strcmp(s,"-"))  return OP_SUB;
    if (!strcmp(s,"*"))  return OP_MUL;
    if (!strcmp(s,"/"))  return OP_DIV;
    if (!strcmp(s,"%"))  return OP_MOD;
    if (!strcmp(s,"&&")) return OP_AND;
    if (!strcmp(s,"||")) return OP_OR;
    if (!strcmp(s,"!"))  return OP_NOT;
    if (!strcmp(s,"==")) return OP_EQ;
    if (!strcmp(s,"!=")) return OP_NEQ;
    if (!strcmp(s,"<"))  return OP_LT;
    if (!strcmp(s,">"))  return OP_GT;
    if (!strcmp(s,"<=")) return OP_LE;
    if (!strcmp(s,">=")) return OP_GE;
    return OP_NONE;
}

static int is_commutative(Operator op) {
    return op == OP_ADD || op == OP_MUL ||
           op == OP_AND || op == OP_OR  ||
           op == OP_EQ  || op == OP_NEQ;
}

/* ── Constant check ── */
static int try_const(const char *s, long *val) {
    if (!s || !s[0]) return 0;
    char *end;
    long v = strtol(s, &end, 10);
    if (end != s && *end == '\0') { *val = v; return 1; }
    return 0;
}

/* ── Variable map ── */
static int var_node(const char *name) {
    for (int i = 0; i < nvar_map; i++)
        if (!strcmp(var_map[i].name, name)) return var_map[i].node_id;
    return -1;
}

static void var_set(const char *name, int node_id) {
    for (int i = 0; i < nvar_map; i++) {
        if (!strcmp(var_map[i].name, name)) {
            var_map[i].node_id = node_id;
            return;
        }
    }
    if (nvar_map < MAX_VARS) {
        strncpy(var_map[nvar_map].name, name, MAX_LABEL_LEN-1);
        var_map[nvar_map].node_id = node_id;
        nvar_map++;
    }
}

/* ── Label helpers ── */
static void node_add_label(Node *n, const char *lbl) {
    for (int i = 0; i < n->nlabels; i++)
        if (!strcmp(n->labels[i], lbl)) return;
    if (n->nlabels < MAX_VARS)
        strncpy(n->labels[n->nlabels++], lbl, MAX_LABEL_LEN-1);
}

/* ──────────────────────────── DAG Builder ────────────────────────────── */

/* Create a new node */
static int new_node(Operator op, int left, int right,
                    int is_const, long cval, const char *leaf) {
    if (ndag >= MAX_NODES) { fprintf(stderr,"DAG overflow\n"); exit(1); }
    Node *n = &dag[ndag];
    memset(n, 0, sizeof *n);
    n->id       = ndag;
    n->op       = op;
    n->left     = left;
    n->right    = right;
    n->is_const = is_const;
    n->const_val= cval;
    n->is_dead  = 0;
    n->nlabels  = 0;
    if (leaf) strncpy(n->leaf_name, leaf, MAX_LABEL_LEN-1);
    return ndag++;
}

/* Find existing interior node (CSE) */
static int find_node(Operator op, int left, int right) {
    for (int i = 0; i < ndag; i++) {
        Node *n = &dag[i];
        if (n->op != op) continue;
        if (n->left == left && n->right == right) return i;
        /* commutative: check swapped */
        if (is_commutative(op) && n->left == right && n->right == left)
            return i;
    }
    return -1;
}

/* Find existing constant node */
static int find_const_node(long val) {
    for (int i = 0; i < ndag; i++)
        if (dag[i].op == OP_NONE && dag[i].is_const && dag[i].const_val == val)
            return i;
    return -1;
}

/* Find existing leaf node (variable) */
static int find_leaf_node(const char *name) {
    /* Return the current mapping first */
    int v = var_node(name);
    if (v >= 0) return v;
    /* Otherwise search leaf nodes */
    for (int i = 0; i < ndag; i++)
        if (dag[i].op == OP_NONE && !dag[i].is_const &&
            !strcmp(dag[i].leaf_name, name))
            return i;
    return -1;
}

/* Get or create a node for an operand (variable or constant) */
static int get_operand_node(const char *name) {
    long cv;
    if (try_const(name, &cv)) {
        int n = find_const_node(cv);
        if (n < 0) n = new_node(OP_NONE, -1, -1, 1, cv, NULL);
        return n;
    }
    int n = find_leaf_node(name);
    if (n < 0) n = new_node(OP_NONE, -1, -1, 0, 0, name);
    return n;
}

/* ── Constant folding ── */
static int try_fold(Operator op, int ln, int rn, long *res) {
    if (ln < 0 || rn < 0) return 0;
    if (!dag[ln].is_const || !dag[rn].is_const) return 0;
    long a = dag[ln].const_val, b = dag[rn].const_val;
    switch (op) {
        case OP_ADD: *res = a + b; return 1;
        case OP_SUB: *res = a - b; return 1;
        case OP_MUL: *res = a * b; return 1;
        case OP_DIV: if (b) { *res = a / b; return 1; } return 0;
        case OP_MOD: if (b) { *res = a % b; return 1; } return 0;
        case OP_LT:  *res = a <  b; return 1;
        case OP_GT:  *res = a >  b; return 1;
        case OP_LE:  *res = a <= b; return 1;
        case OP_GE:  *res = a >= b; return 1;
        case OP_EQ:  *res = a == b; return 1;
        case OP_NEQ: *res = a != b; return 1;
        default: return 0;
    }
}

static int try_fold_unary(Operator op, int ln, long *res) {
    if (ln < 0 || !dag[ln].is_const) return 0;
    long a = dag[ln].const_val;
    if (op == OP_NEG) { *res = -a; return 1; }
    if (op == OP_NOT) { *res = !a; return 1; }
    return 0;
}

/* ── Process one TAC instruction into the DAG ── */
static void dag_process(const Instr *ins) {
    /* ── COPY: result = arg1 ── */
    if (ins->is_copy) {
        int n = get_operand_node(ins->arg1);
        node_add_label(&dag[n], ins->result);
        var_set(ins->result, n);
        return;
    }

    Operator op = parse_op(ins->op_sym);

    /* ── UNARY ── */
    if (ins->is_unary) {
        int ln = get_operand_node(ins->arg1);
        long fv;
        int nn;
        if (try_fold_unary(op, ln, &fv)) {
            int cn = find_const_node(fv);
            if (cn < 0) cn = new_node(OP_NONE,-1,-1,1,fv,NULL);
            node_add_label(&dag[cn], ins->result);
            var_set(ins->result, cn);
        } else {
            nn = find_node(op, ln, -1);
            if (nn < 0) nn = new_node(op, ln, -1, 0, 0, NULL);
            node_add_label(&dag[nn], ins->result);
            var_set(ins->result, nn);
        }
        return;
    }

    /* ── BINARY ── */
    int ln = get_operand_node(ins->arg1);
    int rn = get_operand_node(ins->arg2);
    long fv;
    int nn;

    if (try_fold(op, ln, rn, &fv)) {
        int cn = find_const_node(fv);
        if (cn < 0) cn = new_node(OP_NONE,-1,-1,1,fv,NULL);
        node_add_label(&dag[cn], ins->result);
        var_set(ins->result, cn);
    } else {
        nn = find_node(op, ln, rn);
        if (nn < 0) nn = new_node(op, ln, rn, 0, 0, NULL);
        node_add_label(&dag[nn], ins->result);
        var_set(ins->result, nn);
    }
}

/* ──────────────────────────── Dead-Code Elimination ─────────────────── */
/*
 * A node is live if at least one of its labels is a "live variable"
 * (i.e., used outside the basic block, or used by another live node).
 * For simplicity we treat every user-defined variable as live.
 * Temporaries (t0, t1 … tN) that are not referenced elsewhere are dead.
 */

static int is_temp(const char *s) {
    /* temporaries start with 't' followed by digits */
    if (s[0] != 't') return 0;
    for (int i = 1; s[i]; i++)
        if (!isdigit((unsigned char)s[i])) return 0;
    return s[1] != '\0';
}

static void mark_live(int nid);

static void mark_live(int nid) {
    if (nid < 0 || nid >= ndag) return;
    Node *n = &dag[nid];
    if (!n->is_dead) return;   /* already live */
    n->is_dead = 0;
    mark_live(n->left);
    mark_live(n->right);
}

static void dead_code_elim(void) {
    /* start: mark all nodes dead */
    for (int i = 0; i < ndag; i++) dag[i].is_dead = 1;

    /* mark live: any node whose label is NOT a temp → live */
    for (int i = 0; i < ndag; i++) {
        Node *n = &dag[i];
        for (int j = 0; j < n->nlabels; j++) {
            if (!is_temp(n->labels[j])) {
                mark_live(i);
                break;
            }
        }
    }

    /* Also: leaf nodes (input variables) always live */
    for (int i = 0; i < ndag; i++)
        if (dag[i].op == OP_NONE) dag[i].is_dead = 0;
}

/* ──────────────────────────── Code Generation ────────────────────────── */

static char *node_result_name(int nid) {
    /* Pick the best label: prefer non-temp names */
    Node *n = &dag[nid];
    if (n->is_const) {
        static char buf[64];
        sprintf(buf, "%ld", n->const_val);
        return buf;
    }
    if (n->op == OP_NONE) return n->leaf_name;
    for (int j = 0; j < n->nlabels; j++)
        if (!is_temp(n->labels[j])) return n->labels[j];
    if (n->nlabels > 0) return n->labels[0];
    return "??";
}

static int generated[MAX_NODES];   /* to avoid emitting same node twice */

static void emit(const char *fmt, ...) {
    va_list ap;
    char buf[256];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (nout < MAX_INSTRS)
        strcpy(out_buf[nout++], buf);
}

static void gen_node(int nid) {
    if (nid < 0) return;
    Node *n = &dag[nid];
    if (generated[nid]) return;
    if (n->op == OP_NONE) { generated[nid] = 1; return; } /* leaf */
    if (n->is_dead)       { generated[nid] = 1; return; }

    /* generate children first */
    gen_node(n->left);
    gen_node(n->right);
    generated[nid] = 1;

    char *res = node_result_name(nid);
    char *l   = node_result_name(n->left);

    if (n->op == OP_NEG || n->op == OP_NOT) {
        emit("  %-8s = %s %s", res, op_str[n->op], l);
    } else {
        char *r = node_result_name(n->right);
        emit("  %-8s = %s %s %s", res, l, op_str[n->op], r);
    }

    /* emit copies for extra labels on the same node */
    for (int j = 0; j < n->nlabels; j++) {
        if (strcmp(n->labels[j], res) != 0) {
            emit("  %-8s = %s", n->labels[j], res);
        }
    }
}

static void generate_code(void) {
    memset(generated, 0, sizeof generated);
    /* emit in node creation order (topological since children < parents) */
    for (int i = 0; i < ndag; i++)
        gen_node(i);
}

/* ──────────────────────────── TAC Parser ────────────────────────────── */
/*
 * Supported forms:
 *   result = arg1 op arg2
 *   result = op arg1          (unary)
 *   result = arg1             (copy)
 */

static void parse_line(const char *line) {
    char buf[256];
    strncpy(buf, line, 255);

    /* strip comments */
    char *cm = strstr(buf, "//");
    if (cm) *cm = '\0';

    /* trim */
    char *s = buf;
    while (isspace((unsigned char)*s)) s++;
    int len = (int)strlen(s);
    while (len > 0 && isspace((unsigned char)s[len-1])) s[--len] = '\0';
    if (!len) return;

    Instr ins;
    memset(&ins, 0, sizeof ins);

    /* result = ... */
    char *eq = strchr(s, '=');
    if (!eq) return;
    *eq = '\0';
    char *lhs = s;
    char *rhs = eq + 1;

    /* trim lhs */
    while (isspace((unsigned char)*lhs)) lhs++;
    len = (int)strlen(lhs);
    while (len > 0 && isspace((unsigned char)lhs[len-1])) lhs[--len] = '\0';
    strncpy(ins.result, lhs, MAX_LABEL_LEN-1);

    /* trim rhs */
    while (isspace((unsigned char)*rhs)) rhs++;
    len = (int)strlen(rhs);
    while (len > 0 && isspace((unsigned char)rhs[len-1])) rhs[--len] = '\0';

    /* tokenise rhs */
    char tok[4][MAX_LABEL_LEN];
    int  ntok = 0;
    char *p = rhs;
    while (*p && ntok < 4) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        int i = 0;
        /* multi-char operators */
        if ((*p == '<' || *p == '>' || *p == '!' || *p == '=') && *(p+1) == '=') {
            tok[ntok][0] = *p++; tok[ntok][1] = *p++; tok[ntok][2] = '\0';
        } else if (*p == '&' && *(p+1) == '&') {
            tok[ntok][0] = *p++; tok[ntok][1] = *p++; tok[ntok][2] = '\0';
        } else if (*p == '|' && *(p+1) == '|') {
            tok[ntok][0] = *p++; tok[ntok][1] = *p++; tok[ntok][2] = '\0';
        } else if (isalnum((unsigned char)*p) || *p == '_' || *p == '.') {
            while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '.'))
                tok[ntok][i++] = *p++;
            tok[ntok][i] = '\0';
        } else {
            tok[ntok][0] = *p++; tok[ntok][1] = '\0';
        }
        ntok++;
    }

    if (ntok == 1) {
        /* copy */
        ins.is_copy = 1;
        strncpy(ins.arg1, tok[0], MAX_LABEL_LEN-1);
    } else if (ntok == 2) {
        /* unary: op arg */
        ins.is_unary = 1;
        strncpy(ins.op_sym, tok[0], 7);
        strncpy(ins.arg1,   tok[1], MAX_LABEL_LEN-1);
    } else if (ntok == 3) {
        /* binary: arg1 op arg2 */
        strncpy(ins.arg1,   tok[0], MAX_LABEL_LEN-1);
        strncpy(ins.op_sym, tok[1], 7);
        strncpy(ins.arg2,   tok[2], MAX_LABEL_LEN-1);
    } else return;

    if (ninstrs < MAX_INSTRS) instrs[ninstrs++] = ins;
}

/* ──────────────────────────── Reset ─────────────────────────────────── */

static void reset(void) {
    memset(dag,     0, sizeof dag);      ndag     = 0;
    memset(var_map, 0, sizeof var_map);  nvar_map = 0;
    memset(instrs,  0, sizeof instrs);   ninstrs  = 0;
    memset(out_buf, 0, sizeof out_buf);  nout     = 0;
}

/* ──────────────────────────── Display helpers ───────────────────────── */

static void print_dag(void) {
    printf("  %-4s %-8s %-6s %-4s %-4s %-6s  Labels\n",
           "ID","Op","Const","L","R","Leaf");
    print_separator('-', 58);
    for (int i = 0; i < ndag; i++) {
        Node *n = &dag[i];
        char op_buf[12] = "-";
        if (n->op != OP_NONE) strncpy(op_buf, op_str[n->op], 11);
        char cv_buf[16] = "-";
        if (n->is_const) sprintf(cv_buf, "%ld", n->const_val);
        char l_buf[8]  = "-"; if (n->left  >= 0) sprintf(l_buf,  "%d", n->left);
        char r_buf[8]  = "-"; if (n->right >= 0) sprintf(r_buf,  "%d", n->right);
        printf("  %-4d %-8s %-6s %-4s %-4s %-6s  ",
               n->id, op_buf, cv_buf, l_buf, r_buf,
               n->op == OP_NONE && !n->is_const ? n->leaf_name : "-");
        for (int j = 0; j < n->nlabels; j++)
            printf("%s%s", n->labels[j], j+1<n->nlabels?", ":"");
        if (n->is_dead) printf("  [DEAD]");
        putchar('\n');
    }
}

/* ──────────────────────────── Run one test ───────────────────────────── */

static void run_test(const char *title, const char *code[],
                     int nlines, const char *description) {
    reset();
    printf("\n");
    print_separator('=', 62);
    printf("  TEST : %s\n", title);
    printf("  DESC : %s\n", description);
    print_separator('-', 62);

    /* Parse */
    for (int i = 0; i < nlines; i++) parse_line(code[i]);

    /* Print original */
    printf("  Original TAC (%d instructions):\n", ninstrs);
    for (int i = 0; i < ninstrs; i++) {
        Instr *ins = &instrs[i];
        if (ins->is_copy)
            printf("    %-8s = %s\n", ins->result, ins->arg1);
        else if (ins->is_unary)
            printf("    %-8s = %s %s\n", ins->result, ins->op_sym, ins->arg1);
        else
            printf("    %-8s = %s %s %s\n",
                   ins->result, ins->arg1, ins->op_sym, ins->arg2);
    }
    print_separator('-', 62);

    /* Build DAG */
    for (int i = 0; i < ninstrs; i++) dag_process(&instrs[i]);

    /* Dead-code elimination */
    dead_code_elim();

    /* Print DAG */
    printf("  DAG Nodes:\n");
    print_dag();
    print_separator('-', 62);

    /* Generate optimized code */
    generate_code();

    /* Count savings */
    int dead_cnt = 0;
    for (int i = 0; i < ndag; i++) if (dag[i].is_dead) dead_cnt++;

    printf("  Optimized TAC (%d instructions):\n", nout);
    for (int i = 0; i < nout; i++) printf("%s\n", out_buf[i]);

    printf("\n  ── Optimization Report ──\n");
    printf("    Original instructions : %d\n", ninstrs);
    printf("    Optimized instructions: %d\n", nout);
    printf("    Instructions saved    : %d\n", ninstrs - nout);
    printf("    Dead nodes eliminated : %d\n", dead_cnt);
    print_separator('=', 62);
}

/* ═══════════════════════════════ main ═══════════════════════════════════ */

int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║      DAG-Based Optimization of a Basic Block              ║\n");
    printf("║  Optimizations: CSE | Constant Folding |                  ║\n");
    printf("║                 Dead-Code Elim | Copy Propagation         ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    /* ── Test 1: Common Sub-expression Elimination ── */
    {
        const char *code[] = {
            "t1 = a + b",
            "t2 = a + b",   /* duplicate of t1 */
            "t3 = t1 * c",
            "t4 = t2 * c",  /* duplicate of t3 */
            "x  = t3 + t4"
        };
        run_test("Common Sub-expression Elimination", code, 5,
                 "a+b and (a+b)*c computed twice → CSE removes duplicates");
    }

    /* ── Test 2: Constant Folding ── */
    {
        const char *code[] = {
            "t1 = 3 + 5",
            "t2 = t1 * 2",
            "t3 = 10 - 4",
            "y  = t2 + t3"
        };
        run_test("Constant Folding", code, 4,
                 "All operands are constants → fold at compile time");
    }

    /* ── Test 3: Dead Code Elimination ── */
    {
        const char *code[] = {
            "t1 = a + b",
            "t2 = a - b",   /* t2 never used after this block */
            "t3 = t1 * c",
            "t4 = d + e",   /* t4 never used */
            "z  = t3"
        };
        run_test("Dead Code Elimination", code, 5,
                 "t2 and t4 are computed but never needed → removed");
    }

    /* ── Test 4: Copy Propagation ── */
    {
        const char *code[] = {
            "t1 = a",
            "t2 = t1 + b",
            "t3 = t2",
            "t4 = t3 * c",
            "w  = t4"
        };
        run_test("Copy Propagation", code, 5,
                 "Copy assignments collapse into one node in the DAG");
    }

    /* ── Test 5: Mixed – all optimizations together ── */
    {
        const char *code[] = {
            "t1 = a + b",
            "t2 = a + b",      /* CSE */
            "t3 = 4 * 2",      /* constant fold → 8 */
            "t4 = t1 * t3",
            "t5 = t2 * 8",     /* CSE with t4 (t2≡t1, 8 already folded) */
            "t6 = t4 + t5",    /* t4 ≡ t5 → t6 = 2*t4 */
            "unused = x - y",  /* dead */
            "result = t6"
        };
        run_test("Mixed Optimizations", code, 8,
                 "CSE + constant folding + dead-code + copy propagation");
    }

    /* ── Test 6: Algebraic identities via constant folding ── */
    {
        const char *code[] = {
            "t1 = 100 / 4",
            "t2 = t1 - 5",
            "t3 = t2 * 3",
            "t4 = t3 + 0",    /* +0 folded once t3 is constant */
            "out = t4"
        };
        run_test("Deep Constant Folding Chain", code, 5,
                 "Cascaded constant folding through a chain");
    }

    printf("\nAll tests complete.\n");
    return 0;
}
