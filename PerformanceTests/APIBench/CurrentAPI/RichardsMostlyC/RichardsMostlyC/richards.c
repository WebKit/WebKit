/*  C version of the systems programming language benchmark
**  Author:  M. J. Jordan  Cambridge Computer Laboratory. 
**  
**  Modified by:  M. Richards, Nov 1996
**    to be ANSI C and runnable on 64 bit machines + other minor changes
**  Modified by:  M. Richards, 20 Oct 1998
**    made minor corrections to improve ANSI compliance (suggested
**    by David Levine)
*/

#include <stdio.h>
#include <stdlib.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <QuartzCore/QuartzCore.h>

#if 1
#define                Count           1000
#define                Qpktcountval    2322
#define                Holdcountval    928
#endif

#define                TRUE            1
#define                FALSE           0
#define                MAXINT          32767

#define                BUFSIZE         3
#define                I_IDLE          1
#define                I_WORK          2
#define                I_HANDLERA      3
#define                I_HANDLERB      4
#define                I_DEVA          5
#define                I_DEVB          6
#define                PKTBIT          1
#define                WAITBIT         2
#define                HOLDBIT         4
#define                NOTPKTBIT       !1
#define                NOTWAITBIT      !2
#define                NOTHOLDBIT      0XFFFB

#define                S_RUN           0
#define                S_RUNPKT        1
#define                S_WAIT          2
#define                S_WAITPKT       3
#define                S_HOLD          4
#define                S_HOLDPKT       5
#define                S_HOLDWAIT      6
#define                S_HOLDWAITPKT   7

#define                K_DEV           1000
#define                K_WORK          1001

struct packet
{
    struct packet  *p_link;
    int             p_id;
    int             p_kind;
    int             p_a1;
    char            p_a2[BUFSIZE+1];
};

struct task
{
    struct task    *t_link;
    int             t_id;
    int             t_pri;
    struct packet  *t_wkq;
    int             t_state;
    struct task    *(*t_fn)(struct packet *);
    long            t_v1;
    long            t_v2;
};

char  alphabet[28] = "0ABCDEFGHIJKLMNOPQRSTUVWXYZ";
struct task *tasktab[11]  =  {(struct task *)10,0,0,0,0,0,0,0,0,0,0};
struct task *tasklist    =  0;
struct task *tcb;
long    taskid;
long    v1;
long    v2;
int     qpktcount    =  0;
int     holdcount    =  0;
int     tracing      =  0;
int     layout       =  0;

void append(struct packet *pkt, struct packet *ptr);

void createtask(int id,
                int pri,
                struct packet *wkq,
                int state,
                struct task *(*fn)(struct packet *),
                long v1,
                long v2)
{
    struct task *t = (struct task *)malloc(sizeof(struct task));

    tasktab[id] = t;
    t->t_link   = tasklist;
    t->t_id     = id;
    t->t_pri    = pri;
    t->t_wkq    = wkq;
    t->t_state  = state;
    t->t_fn     = fn;
    t->t_v1     = v1;
    t->t_v2     = v2;
    tasklist    = t;
}

struct packet *pkt(struct packet *link, int id, int kind)
{
    int i;
    struct packet *p = (struct packet *)malloc(sizeof(struct packet));

    for (i=0; i<=BUFSIZE; i++)
        p->p_a2[i] = 0;

    p->p_link = link;
    p->p_id = id;
    p->p_kind = kind;
    p->p_a1 = 0;

    return (p);
}

void trace(char a)
{
   if ( --layout <= 0 )
   {
        layout = 50;
    }

}

void schedule()
{
    while ( tcb != 0 )
    {
        struct packet *pkt;
        struct task *newtcb;

        pkt=0;

        switch ( tcb->t_state )
        {
            case S_WAITPKT:
                pkt = tcb->t_wkq;
                tcb->t_wkq = pkt->p_link;
                tcb->t_state = tcb->t_wkq == 0 ? S_RUN : S_RUNPKT;

            case S_RUN:
            case S_RUNPKT:
                taskid = tcb->t_id;
                v1 = tcb->t_v1;
                v2 = tcb->t_v2;
                if (tracing) {
          trace(taskid+'0');
        }
                newtcb = (*(tcb->t_fn))(pkt);
                tcb->t_v1 = v1;
                tcb->t_v2 = v2;
                tcb = newtcb;
                break;

            case S_WAIT:
            case S_HOLD:
            case S_HOLDPKT:
            case S_HOLDWAIT:
            case S_HOLDWAITPKT:
                tcb = tcb->t_link;
                break;

            default:
                return;
        }
    }
}

struct task *wait_(void)
{
    tcb->t_state |= WAITBIT;
    return (tcb);
}

struct task *holdself(void)
{
    ++holdcount;
    tcb->t_state |= HOLDBIT;
    return (tcb->t_link) ;
}

struct task *findtcb(int id)
{
    struct task *t = 0;

    if (1<=id && id<=(long)tasktab[0])
    t = tasktab[id];
    return(t);
}

struct task *release(int id)
{
    struct task *t;

    t = findtcb(id);
    if ( t==0 ) return (0);

    t->t_state &= NOTHOLDBIT;
    if ( t->t_pri > tcb->t_pri ) return (t);

    return (tcb) ;
}


struct task *qpkt(struct packet *pkt)
{
    struct task *t;

    t = findtcb(pkt->p_id);
    if (t==0) return (t);

    qpktcount++;

    pkt->p_link = 0;
    pkt->p_id = (int)taskid;

   if (t->t_wkq==0)
    {
        t->t_wkq = pkt;
        t->t_state |= PKTBIT;
        if (t->t_pri > tcb->t_pri) return (t);
    }
    else
    {
        append(pkt, (struct packet *)&(t->t_wkq));
    }

    return (tcb);
}

struct task *idlefn(struct packet *pkt)
{
    if ( --v2==0 ) return ( holdself() );

    if ( (v1&1) == 0 )
    {
        v1 = ( v1>>1) & MAXINT;
        return ( release(I_DEVA) );
    }
    else
    {
        v1 = ( (v1>>1) & MAXINT) ^ 0XD008;
        return ( release(I_DEVB) );
    }
}

static JSGlobalContextRef ctx;
static JSObjectRef jsWorkfn;
#define CHECK_EXCEPTION(exception) \
    if (exception) { \
        JSStringRef message = JSValueToStringCopy(ctx, exception, NULL); \
        size_t length = JSStringGetMaximumUTF8CStringSize(message); \
        char* buffer = (char*)malloc(length + 1); \
        JSStringGetUTF8CString(message, buffer, length + 1); \
        buffer[length] = 0; \
        fprintf(stderr, "uncaught exception: %s\n", buffer); \
        exit(1); \
    } \
    do { } while (0)

#define CHECKED(...) \
    __VA_ARGS__; \
    CHECK_EXCEPTION(exception);

#define CONCAT_(A, B) A ## B
#define CONCAT(A, B) CONCAT_(A, B)

#define CHECKED_INT_(target, tmp, ...) \
    JSValueRef tmp = CHECKED(__VA_ARGS__); \
    target = CHECKED(JSValueToNumber(ctx, tmp, &exception))

#define CHECKED_INT(target, ...) \
    CHECKED_INT_(target, CONCAT(tmp, __COUNTER__), __VA_ARGS__)

struct task* valueToTask(JSValueRef value)
{
    JSValueRef exception = NULL;
    double d = CHECKED(JSValueToNumber(ctx, value, &exception));
    return (struct task*)(intptr_t)d;
}

JSValueRef waitWrapper(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    struct task* task = wait_();
    return JSValueMakeNumber(ctx, (intptr_t)task);
}

JSValueRef qpktWrapper(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    struct packet* pkt = (struct packet*)(intptr_t)JSValueToNumber(ctx, arguments[0], exception);
    CHECK_EXCEPTION(*exception);
    struct task* task = qpkt(pkt);
    return JSValueMakeNumber(ctx, (intptr_t)task);
}

JSStringRef readFile(const char* path)
{
    FILE* file = fopen(path, "r");
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    char* buffer = calloc(size, sizeof(char));
    fseek(file, 0, SEEK_SET);
    fread(buffer, sizeof(char), size, file);
    fclose(file);
    JSStringRef contents = JSStringCreateWithUTF8CString(buffer);
    free(buffer);
    return contents;
}

struct task *workfn(struct packet *pkt)
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        ctx = JSGlobalContextCreate(NULL);
        JSObjectRef globalObject = JSContextGetGlobalObject(ctx);
        
        JSStringRef source = readFile("richards.js");
        JSStringRef workfnString = JSStringCreateWithUTF8CString("createWorkfn");
        JSStringRef waitString = JSStringCreateWithUTF8CString("wait");
    
        JSEvaluateScript(ctx, source, NULL, NULL, 0, NULL);
        
        JSObjectRef jsAlphabet = JSObjectMakeArray(ctx, 0, NULL, NULL);
        for (unsigned i = 0; i < 28; ++i)
            JSObjectSetPropertyAtIndex(ctx, jsAlphabet, i, JSValueMakeNumber(ctx, alphabet[i]), NULL);
        
        JSObjectRef createWorkfn = JSValueToObject(ctx, JSObjectGetProperty(ctx, globalObject, workfnString, NULL), NULL);
        JSValueRef args[6] = {
            JSObjectMakeFunctionWithCallback(ctx, waitString, waitWrapper),
            JSValueMakeNumber(ctx, I_HANDLERA),
            JSValueMakeNumber(ctx, I_HANDLERB),
            JSValueMakeNumber(ctx, BUFSIZE),
            jsAlphabet,
        };
        JSValueRef result = JSObjectCallAsFunction(ctx, createWorkfn, NULL, 5, args, NULL);
        jsWorkfn = JSValueToObject(ctx, result, NULL);
        
        JSStringRelease(source);
        JSStringRelease(workfnString);
        JSStringRelease(waitString);
    });
    
    JSValueRef exception = NULL;
    JSValueRef args[3] = { JSValueMakeNumber(ctx, (intptr_t)pkt), JSValueMakeNumber(ctx, v1), JSValueMakeNumber(ctx, v1) };
    JSValueRef result = CHECKED(JSObjectCallAsFunction(ctx, jsWorkfn, NULL, 3, args, &exception));
    JSObjectRef resultObject = CHECKED(JSValueToObject(ctx, result, &exception));
    if (!JSValueIsArray(ctx, result))
        return valueToTask(result);
   
    CHECKED_INT(v1, JSObjectGetPropertyAtIndex(ctx, resultObject, 0, &exception));
    CHECKED_INT(v2, JSObjectGetPropertyAtIndex(ctx, resultObject, 1, &exception));
    CHECKED_INT(pkt->p_a1, JSObjectGetPropertyAtIndex(ctx, resultObject, 2, &exception));
    pkt->p_id = (int)v1;
    JSValueRef data = CHECKED(JSObjectGetPropertyAtIndex(ctx, resultObject, 3, &exception));
    JSObjectRef dataObject = CHECKED(JSValueToObject(ctx, data, &exception));
    for (unsigned i = 0; i < BUFSIZE; ++i) {
         CHECKED_INT(pkt->p_a2[i], JSObjectGetPropertyAtIndex(ctx, dataObject, i, &exception));
    }
    return qpkt(pkt);
}

struct task *handlerfn(struct packet *pkt)
{
  if ( pkt!=0) {
    append(pkt, (struct packet *)(pkt->p_kind==K_WORK ? &v1 : &v2));
  }

  if ( v1!=0 ) {
    int count;
    struct packet *workpkt = (struct packet *)v1;
    count = workpkt->p_a1;

    if ( count > BUFSIZE ) {
      v1 = (long)(((struct packet *)v1)->p_link);
      return ( qpkt(workpkt) );
    }

    if ( v2!=0 ) {
      struct packet *devpkt;

      devpkt = (struct packet *)v2;
      v2 = (long)(((struct packet *)v2)->p_link);
      devpkt->p_a1 = workpkt->p_a2[count];
      workpkt->p_a1 = count+1;
      return( qpkt(devpkt) );
    }
  }
  return wait_();
}

struct task *devfn(struct packet *pkt)
{
    if ( pkt==0 )
    {
        if ( v1==0 ) return ( wait_() );
        pkt = (struct packet *)v1;
        v1 = 0;
        return ( qpkt(pkt) );
    }
    else
    {
        v1 = (long)pkt;
        if (tracing) trace(pkt->p_a1);
        return ( holdself() );
    }
}

void append(struct packet *pkt, struct packet *ptr)
{
    pkt->p_link = 0;

    while ( ptr->p_link ) ptr = ptr->p_link;

    ptr->p_link = pkt;
}

void setup()
{
    struct packet *wkq = 0;

    createtask(I_IDLE, 0, wkq, S_RUN, idlefn, 1, Count);

    wkq = pkt(0, 0, K_WORK);
    wkq = pkt(wkq, 0, K_WORK);

    createtask(I_WORK, 1000, wkq, S_WAITPKT, workfn, I_HANDLERA, 0);

    wkq = pkt(0, I_DEVA, K_DEV);
    wkq = pkt(wkq, I_DEVA, K_DEV);
    wkq = pkt(wkq, I_DEVA, K_DEV);

    createtask(I_HANDLERA, 2000, wkq, S_WAITPKT, handlerfn, 0, 0);

    wkq = pkt(0, I_DEVB, K_DEV);
    wkq = pkt(wkq, I_DEVB, K_DEV);
    wkq = pkt(wkq, I_DEVB, K_DEV);

    createtask(I_HANDLERB, 3000, wkq, S_WAITPKT, handlerfn, 0, 0);

    wkq = 0;
    createtask(I_DEVA, 4000, wkq, S_WAIT, devfn, 0, 0);
    createtask(I_DEVB, 5000, wkq, S_WAIT, devfn, 0, 0);

    tcb = tasklist;

    qpktcount = holdcount = 0;

    tracing = FALSE;
    layout = 0;
}

int getQpktcount()
{
    return  qpktcount;
}

int getHoldcount()
{
    return holdcount;
}

int main()
{
    const unsigned iterations = 50;
    const CFTimeInterval start = CACurrentMediaTime();
    for (unsigned i = 0; i < iterations; ++i) {
        setup();
        schedule();
        if (qpktcount != Qpktcountval || holdcount != Holdcountval) {
            fprintf(stderr, "Error during execution: queueCount = %d, holdCount = %d.\n", qpktcount, holdcount);
            exit(1);
        }
    }
    const CFTimeInterval end = CACurrentMediaTime();
    printf("%d\n", (int)((end-start) * 1000));
    return 0;
}

