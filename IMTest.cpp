#include <stdlib.h>
#include <stdio.h>
#include <locale.h>

#include <string.h>
#include <unistd.h>

#include <X11/X.h> 
#include <X11/Xlib.h>

#define LOG_HERE() printf("%s-%d\n", __func__, __LINE__)

#define DEFAULT_SIZE 8

static int preedit_state_callback( XIM xim, XPointer client_data,
                                   XIMPreeditStateNotifyCallbackStruct *call_data )
{
    LOG_HERE();
    return 1;
}

static int preedit_start_callback( XIM xim, XPointer client_data, XPointer call_data )
{
    LOG_HERE();
    return -1;
}

static void preedit_done_callback( XIM xim, XPointer client_data, XPointer call_data )
{
    LOG_HERE();
}

static void preedit_draw_callback( XIM xim, XPointer client_data, XIMPreeditDrawCallbackStruct *call_data )
{
    XIMText *xim_text = call_data->text;

    LOG_HERE();

    if (xim_text != NULL)
    {
        printf( "caret %d first %d, length %2d string '%s' \n",
                call_data->caret, call_data->chg_first, call_data->chg_length, xim_text->string.multi_byte );
    }
    else
    {
        printf( "caret %d first %d, length %2d string '' \n",
                call_data->caret, call_data->chg_first, call_data->chg_length );
    }
}

static void preedit_caret_callback( XIM xim, XPointer client_data, XIMPreeditCaretCallbackStruct *call_data )
{
    LOG_HERE();
    if (call_data != NULL)
    {
        printf( "direction: %d position: %d \n", call_data->direction, call_data->position );
    }
}

static void moveCandiateWindow(XIC xic, int x, int y)
{
    XVaNestedList preedit_attr;
    XPoint nspot;

    nspot.x = x;
    nspot.y = y;
    preedit_attr = XVaCreateNestedList(0, XNSpotLocation, &nspot, NULL);
    XSetICValues(xic, XNPreeditAttributes, preedit_attr, NULL);
    
    XFree(preedit_attr);
}

static char* processKey(XIC xic, XEvent* pEvent, char* pBuff, int* nSize)
{
    Status status;
    KeySym keysym;

    *nSize = XmbLookupString( xic, &pEvent->xkey, pBuff, *nSize - 1, &keysym, &status );
    if (status == XBufferOverflow)
    {
        printf( "LookupString size: %d realloc: %d\n", *nSize, *nSize + 1 );
        pBuff = (char*)realloc( pBuff, *nSize + 1 );
        memset(pBuff, 0, *nSize+1);
        *nSize = XmbLookupString( xic, &pEvent->xkey, pBuff, *nSize, &keysym, &status );
    }
    
    if (pEvent->type == KeyPress)
    {
        printf( "LookupString:: status %d keycode %4u keysym 0x%08lx size %2d string '%s'\n",
                    status, pEvent->xkey.keycode, keysym, *nSize, pBuff );
    }
    
    return pBuff;
}

static XIC createXIC(XIM xim, Window win)
{
    XIC xic;
    XVaNestedList preedit_attr;
    XIMStyle input_style = 0;
    
    XIMCallback state_callback = { NULL, (XIMProc)preedit_state_callback };
    XIMCallback draw_callback  = { NULL, (XIMProc)preedit_draw_callback  };
    XIMCallback start_callback = { NULL, (XIMProc)preedit_start_callback };
    XIMCallback done_callback  = { NULL, (XIMProc)preedit_done_callback  };
    XIMCallback caret_callback = { NULL, (XIMProc)preedit_caret_callback };

    preedit_attr = XVaCreateNestedList( 0,
                                        XNPreeditStateNotifyCallback, &state_callback,
                                        XNPreeditStartCallback,       &start_callback,
                                        XNPreeditDoneCallback,        &done_callback,
                                        XNPreeditDrawCallback,        &draw_callback,
                                        XNPreeditCaretCallback,       &caret_callback,
                                        NULL );

    /**
     * XIMPreeditCallbacks:
     * 在ubuntu/kylin上，设置的回调函数都不调用。
     * 在ubuntu上，会阻止候选框移动(即导致XNSpotLocation设置无效)。
     * 所以考虑到通用，必须使用XIMPreeditNothing
     */
    input_style = XIMPreeditNothing   | XIMStatusNothing;
    input_style = XIMPreeditCallbacks | XIMStatusCallbacks;
    xic = XCreateIC( xim,
                     XNInputStyle,        input_style,
                     XNClientWindow,      win,
                     XNPreeditAttributes, preedit_attr,
                     NULL );
                     
    XFree(preedit_attr);

    return xic;
}

static void loopEvent(XIC xic, Display *dpy)
{
    char *pBuff;
    int nSize = DEFAULT_SIZE;
    XEvent event;
    Bool ret;

    pBuff = (char*)malloc( nSize );
    memset(pBuff, 0, nSize);

    int candidate_x = 10;
    int candidate_y = 10;

    for (;;)
    {
        XNextEvent( dpy, &event );
        switch (event.type)
        {
        case KeyPress:
            //LOG_HERE();
            //printf( "filter << xevent type %2d window 0x%08lx state 0x%08x keycode %4u ",
            //        event.xkey.type, event.xkey.window, event.xkey.state, event.xkey.keycode );
            moveCandiateWindow(xic, candidate_x, candidate_y);
            pBuff = processKey(xic, &event, pBuff, &nSize);
            break;

        case KeyRelease:
            //LOG_HERE();
            // 忽略空格
            if (event.xkey.keycode != 65)
            {
                candidate_x += 10;
            }
            pBuff = processKey(xic, &event, pBuff, &nSize);
            break;

        case ClientMessage:
            //printf( "filter << xevent type %2d window 0x%08lx ", event.xkey.type, event.xkey.window );
            //LOG_HERE();
            break;
        }

        ret = XFilterEvent( &event, None );

        if (ret == True)
        {
            continue;
        }
    }

    free(pBuff);
}

int main(int argc, char** argv)
{
    Display *dpy;
    Window win;
    XIM xim;
    XIC xic;

    setlocale( LC_CTYPE, "" );
    XSetLocaleModifiers( "" );

    dpy = XOpenDisplay( NULL );
    win = XCreateSimpleWindow( dpy, XDefaultRootWindow( dpy ), 0, 0, 300, 300, 5, 0, 0 );
    XMapWindow( dpy, win );

    xim = XOpenIM( dpy, NULL, NULL, NULL );
    xic = createXIC(xim, win);
   
    XSetICFocus( xic );
    XSelectInput( dpy, win, KeyPressMask | KeyReleaseMask );

    /* 进入事件循环 */
    loopEvent(xic, dpy);
    
    /* 销毁资源？ */
    
    return 0;
}

