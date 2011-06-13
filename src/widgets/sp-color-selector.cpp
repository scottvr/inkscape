/*
 *   bulia byak <buliabyak@users.sf.net>
 *   Jon A. Cruz <jon@joncruz.org>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <math.h>
#include <gtk/gtk.h>
#include <glibmm/i18n.h>
#include "sp-color-selector.h"

enum {
    GRABBED,
    DRAGGED,
    RELEASED,
    CHANGED,
    LAST_SIGNAL
};

#define noDUMP_CHANGE_INFO
#define FOO_NAME(x) g_type_name( G_TYPE_FROM_INSTANCE(x) )

static void sp_color_selector_class_init( SPColorSelectorClass *klass );
static void sp_color_selector_init( SPColorSelector *csel );
static void sp_color_selector_destroy( GtkObject *object );

static void sp_color_selector_show_all( GtkWidget *widget );
static void sp_color_selector_hide_all( GtkWidget *widget );

static GtkVBoxClass *parent_class;
static guint csel_signals[LAST_SIGNAL] = {0};

double ColorSelector::_epsilon = 1e-4;

GType sp_color_selector_get_type( void )
{
    static GType type = 0;
    if (!type) {
        static const GTypeInfo info = {
            sizeof(SPColorSelectorClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc) sp_color_selector_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(SPColorSelector),
            0,    /* n_preallocs */
            (GInstanceInitFunc) sp_color_selector_init,
            NULL
        };

        type = g_type_register_static( GTK_TYPE_VBOX,
                                       "SPColorSelector",
                                       &info,
                                       static_cast<GTypeFlags>(0) );
    }
    return type;
}

void sp_color_selector_class_init( SPColorSelectorClass *klass )
{
    static const gchar* nameset[] = {N_("Unnamed"), 0};
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = GTK_OBJECT_CLASS(klass);
    widget_class = GTK_WIDGET_CLASS(klass);

    parent_class = GTK_VBOX_CLASS( gtk_type_class(GTK_TYPE_VBOX) );

    csel_signals[GRABBED] = g_signal_new( "grabbed",
                                            G_TYPE_FROM_CLASS(object_class),
                                            (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE),
                                            G_STRUCT_OFFSET(SPColorSelectorClass, grabbed),
					    NULL, NULL,
                                            gtk_marshal_NONE__NONE,
                                            GTK_TYPE_NONE, 0 );
    csel_signals[DRAGGED] = g_signal_new( "dragged",
                                            G_TYPE_FROM_CLASS(object_class),
                                            (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE),
                                            G_STRUCT_OFFSET(SPColorSelectorClass, dragged),
					    NULL, NULL,
                                            gtk_marshal_NONE__NONE,
                                            GTK_TYPE_NONE, 0 );
    csel_signals[RELEASED] = g_signal_new( "released",
                                             G_TYPE_FROM_CLASS(object_class),
                                             (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE),
                                             G_STRUCT_OFFSET(SPColorSelectorClass, released),
					     NULL, NULL,
                                             gtk_marshal_NONE__NONE,
                                             GTK_TYPE_NONE, 0 );
    csel_signals[CHANGED] = g_signal_new( "changed",
                                            G_TYPE_FROM_CLASS(object_class),
                                            (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE),
                                            G_STRUCT_OFFSET(SPColorSelectorClass, changed),
					    NULL, NULL,
                                            gtk_marshal_NONE__NONE,
                                            GTK_TYPE_NONE, 0 );

    klass->name = nameset;
    klass->submode_count = 1;

    object_class->destroy = sp_color_selector_destroy;

    widget_class->show_all = sp_color_selector_show_all;
    widget_class->hide_all = sp_color_selector_hide_all;

}

void sp_color_selector_init( SPColorSelector *csel )
{
    if ( csel->base )
    {
        csel->base->init();
    }
/*   g_signal_connect(G_OBJECT(csel->rgbae), "changed", G_CALLBACK(sp_color_selector_rgba_entry_changed), csel); */
}

void sp_color_selector_destroy( GtkObject *object )
{
    SPColorSelector *csel = SP_COLOR_SELECTOR( object );
    if ( csel->base )
        {
            delete csel->base;
            csel->base = 0;
        }

    if ( (GTK_OBJECT_CLASS(parent_class))->destroy ) {
        (* (GTK_OBJECT_CLASS(parent_class))->destroy)(object);
    }
}

void sp_color_selector_show_all( GtkWidget *widget )
{
    gtk_widget_show( widget );
}

void sp_color_selector_hide_all( GtkWidget *widget )
{
    gtk_widget_hide( widget );
}

GtkWidget *sp_color_selector_new( GType selector_type )
{
    g_return_val_if_fail( g_type_is_a( selector_type, SP_TYPE_COLOR_SELECTOR ), NULL );

    SPColorSelector *csel = SP_COLOR_SELECTOR( g_object_new( selector_type, NULL ) );

    return GTK_WIDGET( csel );
}

void ColorSelector::setSubmode( guint /*submode*/ )
{
}

guint ColorSelector::getSubmode() const
{
    guint mode = 0;
    return mode;
}

ColorSelector::ColorSelector( SPColorSelector* csel )
    : _csel(csel),
      _color( 0 ),
      _alpha(1.0),
      _held(FALSE),
      virgin(true)
{
    g_return_if_fail( SP_IS_COLOR_SELECTOR(_csel) );
}

ColorSelector::~ColorSelector()
{
}

void ColorSelector::init()
{
    _csel->base = new ColorSelector( _csel );
}

void ColorSelector::setColor( const SPColor& color )
{
    setColorAlpha( color, _alpha );
}

SPColor ColorSelector::getColor() const
{
    return _color;
}

void ColorSelector::setAlpha( gfloat alpha )
{
    g_return_if_fail( ( 0.0 <= alpha ) && ( alpha <= 1.0 ) );
    setColorAlpha( _color, alpha );
}

gfloat ColorSelector::getAlpha() const
{
    return _alpha;
}

#include "svg/svg-icc-color.h"

/**
Called from the outside to set the color; optionally emits signal (only when called from
downstream, e.g. the RGBA value field, but not from the rest of the program)
*/
void ColorSelector::setColorAlpha( const SPColor& color, gfloat alpha, bool emit )
{
#ifdef DUMP_CHANGE_INFO
    g_message("ColorSelector::setColorAlpha( this=%p, %f, %f, %f, %s,   %f,   %s) in %s", this, color.v.c[0], color.v.c[1], color.v.c[2], (color.icc?color.icc->colorProfile.c_str():"<null>"), alpha, (emit?"YES":"no"), FOO_NAME(_csel));
#endif
    g_return_if_fail( _csel != NULL );
    g_return_if_fail( ( 0.0 <= alpha ) && ( alpha <= 1.0 ) );

#ifdef DUMP_CHANGE_INFO
    g_message("---- ColorSelector::setColorAlpha    virgin:%s   !close:%s    alpha is:%s in %s",
              (virgin?"YES":"no"),
              (!color.isClose( _color, _epsilon )?"YES":"no"),
              ((fabs((_alpha) - (alpha)) >= _epsilon )?"YES":"no"),
              FOO_NAME(_csel)
              );
#endif

    if ( virgin || !color.isClose( _color, _epsilon ) ||
         (fabs((_alpha) - (alpha)) >= _epsilon )) {

        virgin = false;

        _color = color;
        _alpha = alpha;
        _colorChanged();

        if (emit) {
            g_signal_emit(G_OBJECT(_csel), csel_signals[CHANGED], 0);
        }
#ifdef DUMP_CHANGE_INFO
    } else {
        g_message("++++ ColorSelector::setColorAlpha   color:%08x  ==>  _color:%08X   isClose:%s   in %s", color.toRGBA32(alpha), _color.toRGBA32(_alpha),
                  (color.isClose( _color, _epsilon )?"YES":"no"), FOO_NAME(_csel));
#endif
    }
}

void ColorSelector::_grabbed()
{
    _held = TRUE;
#ifdef DUMP_CHANGE_INFO
    g_message("%s:%d: About to signal %s in %s", __FILE__, __LINE__,
               "GRABBED",
               FOO_NAME(_csel));
#endif
    g_signal_emit(G_OBJECT(_csel), csel_signals[GRABBED], 0);
}

void ColorSelector::_released()
{
    _held = false;
#ifdef DUMP_CHANGE_INFO
    g_message("%s:%d: About to signal %s in %s", __FILE__, __LINE__,
               "RELEASED",
               FOO_NAME(_csel));
#endif
    g_signal_emit(G_OBJECT(_csel), csel_signals[RELEASED], 0);
    g_signal_emit(G_OBJECT(_csel), csel_signals[CHANGED], 0);
}

// Called from subclasses to update color and broadcast if needed
void ColorSelector::_updateInternals( const SPColor& color, gfloat alpha, gboolean held )
{
    g_return_if_fail( ( 0.0 <= alpha ) && ( alpha <= 1.0 ) );
    gboolean colorDifferent = ( !color.isClose( _color, _epsilon )
                                || ( fabs((_alpha) - (alpha)) >= _epsilon ) );

    gboolean grabbed = held && !_held;
    gboolean released = !held && _held;

    // Store these before emmiting any signals
    _held = held;
    if ( colorDifferent )
    {
        _color = color;
        _alpha = alpha;
    }

    if ( grabbed )
    {
#ifdef DUMP_CHANGE_INFO
        g_message("%s:%d: About to signal %s to color %08x::%s in %s", __FILE__, __LINE__,
                   "GRABBED",
                   color.toRGBA32( alpha ), (color.icc?color.icc->colorProfile.c_str():"<null>"), FOO_NAME(_csel));
#endif
        g_signal_emit(G_OBJECT(_csel), csel_signals[GRABBED], 0);
    }
    else if ( released )
    {
#ifdef DUMP_CHANGE_INFO
        g_message("%s:%d: About to signal %s to color %08x::%s in %s", __FILE__, __LINE__,
                   "RELEASED",
                   color.toRGBA32( alpha ), (color.icc?color.icc->colorProfile.c_str():"<null>"), FOO_NAME(_csel));
#endif
        g_signal_emit(G_OBJECT(_csel), csel_signals[RELEASED], 0);
    }

    if ( colorDifferent || released )
    {
#ifdef DUMP_CHANGE_INFO
        g_message("%s:%d: About to signal %s to color %08x::%s in %s", __FILE__, __LINE__,
                   (_held ? "CHANGED" : "DRAGGED" ),
                   color.toRGBA32( alpha ), (color.icc?color.icc->colorProfile.c_str():"<null>"), FOO_NAME(_csel));
#endif
        g_signal_emit(G_OBJECT(_csel), csel_signals[_held ? CHANGED : DRAGGED], 0);
    }
}

/**
 * Called once the color actually changes. Allows subclasses to react to changes.
 */
void ColorSelector::_colorChanged()
{
}

void ColorSelector::getColorAlpha( SPColor &color, gfloat &alpha ) const
{
    gint i = 0;

    color = _color;
    alpha = _alpha;

    // Try to catch uninitialized value usage
    if ( color.v.c[0] )
    {
        i++;
    }
    if ( color.v.c[1] )
    {
        i++;
    }
    if ( color.v.c[2] )
    {
        i++;
    }
    if ( alpha )
    {
        i++;
    }
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
