/**
 * @file
 * XML editor.
 */
/* Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   MenTaLguY <mental@rydia.net>
 *   bulia byak <buliabyak@users.sf.net>
 *   Johan Engelen <goejendaagh@zonnet.nl>
 *   David Turner
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 1999-2006 Authors
 * Released under GNU GPL, read the file 'COPYING' for more information
 *
 */

#include "xml-tree.h"
#include "widgets/icon.h"
#include <gdk/gdkkeysyms.h>
#include <glibmm/i18n.h>
#include <gtkmm/stock.h>

#include "desktop.h"
#include "desktop-handles.h"
#include "dialogs/dialog-events.h"
#include "document.h"
#include "document-undo.h"
#include "event-context.h"
#include "helper/window.h"
#include "inkscape.h"
#include "interface.h"
#include "macros.h"
#include "message-context.h"
#include "message-stack.h"
#include "preferences.h"
#include "selection.h"
#include "shortcuts.h"
#include "sp-root.h"
#include "sp-string.h"
#include "sp-tspan.h"
#include "ui/icon-names.h"
#include "verbs.h"
#include "widgets/icon.h"

#include "widgets/sp-xmlview-attr-list.h"
#include "widgets/sp-xmlview-content.h"
#include "widgets/sp-xmlview-tree.h"

namespace Inkscape {
namespace UI {
namespace Dialog {

XmlTree::XmlTree (void) :
    UI::Widget::Panel ("", "/dialogs/xml/", SP_VERB_DIALOG_XML_EDITOR),
    blocked (0),
    _message_stack (NULL),
    _message_context (NULL),
    current_desktop (NULL),
    current_document (NULL),
    selected_attr (0),
    selected_repr (NULL),
    tree (NULL),
    attributes (NULL),
    content (NULL),
    attr_name (),
    attr_value (),
    status (""),
    tree_toolbar(),
    xml_element_new_button ( _("New element node")),
    xml_text_new_button ( _("New text node")),
    xml_node_delete_button ( Q_("nodeAsInXMLdialogTooltip|Delete node")),
    xml_node_duplicate_button ( _("Duplicate node")),
    unindent_node_button (Gtk::Stock::UNINDENT),
    indent_node_button (Gtk::Stock::INDENT),
    raise_node_button (Gtk::Stock::GO_UP),
    lower_node_button (Gtk::Stock::GO_DOWN),
    attr_toolbar(),
    xml_attribute_delete_button (_("Delete attribute")),
    text_container (),
    attr_container (),
    attr_subpaned_container (),
    set_attr (_("Set")),
    new_window(NULL)
{

    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    if (!desktop) {
        return;
    }

    Gtk::Box *contents = _getContents();
    contents->set_spacing(0);
    contents->set_size_request(320, 260);

    status.set_alignment( 0.0, 0.5);
    status.set_size_request(1, -1);
    status.set_markup("");
    status_box.pack_start( status, TRUE, TRUE, 0);
    contents->pack_end(status_box, false, false, 2);

    paned.set_position(256);
    contents->pack_start(paned, TRUE, TRUE, 0);

    _message_stack = new Inkscape::MessageStack();
    _message_context = new Inkscape::MessageContext(_message_stack);
    _message_changed_connection = _message_stack->connectChanged(
            sigc::bind(sigc::ptr_fun(_set_status_message), GTK_WIDGET(status.gobj())));

    /* tree view */
    paned.pack1(left_box);

    tree = SP_XMLVIEW_TREE(sp_xmlview_tree_new(NULL, NULL, NULL));
    gtk_widget_set_tooltip_text( GTK_WIDGET(tree), _("Drag to reorder nodes") );

    tree_toolbar.set_toolbar_style(Gtk::TOOLBAR_ICONS);
    xml_element_new_button.set_icon_widget(*Gtk::manage(Glib::wrap(
            sp_icon_new (Inkscape::ICON_SIZE_LARGE_TOOLBAR, INKSCAPE_ICON("xml-element-new")))) );
    xml_element_new_button.set_tooltip_text(_("New element node"));
    xml_element_new_button.set_sensitive(false);
    tree_toolbar.add(xml_element_new_button);

    xml_text_new_button.set_icon_widget(*Gtk::manage(Glib::wrap(
            sp_icon_new (Inkscape::ICON_SIZE_LARGE_TOOLBAR, INKSCAPE_ICON("xml-text-new")))));
    xml_text_new_button.set_tooltip_text(_("New text node"));
    xml_text_new_button.set_sensitive(false);
    tree_toolbar.add(xml_text_new_button);

    xml_node_duplicate_button.set_icon_widget(*Gtk::manage(Glib::wrap(
            sp_icon_new (Inkscape::ICON_SIZE_LARGE_TOOLBAR, INKSCAPE_ICON("xml-node-duplicate")))));
    xml_node_duplicate_button.set_tooltip_text(_("Duplicate node"));
    xml_node_duplicate_button.set_sensitive(false);
    tree_toolbar.add(xml_node_duplicate_button);

    tree_toolbar.add(separator);

    xml_node_delete_button.set_icon_widget(*Gtk::manage(Glib::wrap(
            sp_icon_new (Inkscape::ICON_SIZE_LARGE_TOOLBAR, INKSCAPE_ICON("xml-node-delete")))));
    xml_node_delete_button.set_tooltip_text(Q_("nodeAsInXMLdialogTooltip|Delete node"));
    xml_node_delete_button.set_sensitive(false);
    tree_toolbar.add(xml_node_delete_button);

    tree_toolbar.add(separator2);

    unindent_node_button.set_label(_("Unindent node"));
    unindent_node_button.set_tooltip_text(_("Unindent node"));
    unindent_node_button.set_sensitive(false);
    tree_toolbar.add(unindent_node_button);

    indent_node_button.set_label(_("Indent node"));
    indent_node_button.set_tooltip_text(_("Indent node"));
    indent_node_button.set_sensitive(false);
    tree_toolbar.add(indent_node_button);

    raise_node_button.set_label(_("Raise node"));
    raise_node_button.set_tooltip_text(_("Raise node"));
    raise_node_button.set_sensitive(false);
    tree_toolbar.add(raise_node_button);

    lower_node_button.set_label(_("Lower node"));
    lower_node_button.set_tooltip_text(_("Lower node"));
    lower_node_button.set_sensitive(false);
    tree_toolbar.add(lower_node_button);

    left_box.pack_start(tree_toolbar, FALSE, TRUE, 0);

    Gtk::ScrolledWindow *tree_scroller = new Gtk::ScrolledWindow();
    tree_scroller->set_policy( Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC );
    tree_scroller->add(*Gtk::manage(Glib::wrap(GTK_WIDGET(tree))));

    left_box.pack_start(*tree_scroller);

    /* node view */
    paned.pack2(right_box);

    /* attributes */
    right_box.pack_start( attr_container, TRUE, TRUE, 0 );

    attributes = SP_XMLVIEW_ATTR_LIST(sp_xmlview_attr_list_new(NULL));

    attr_toolbar.set_toolbar_style(Gtk::TOOLBAR_ICONS);
    xml_attribute_delete_button.set_icon_widget(*Gtk::manage(Glib::wrap(sp_icon_new (Inkscape::ICON_SIZE_LARGE_TOOLBAR, INKSCAPE_ICON("xml-attribute-delete")))));
    xml_attribute_delete_button.set_tooltip_text(_("Delete attribute"));
    xml_attribute_delete_button.set_sensitive(false);
    attr_toolbar.add(xml_attribute_delete_button);

    attr_container.pack_start( attr_toolbar, FALSE, TRUE, 0 );
    attr_container.pack_start( attr_subpaned_container, TRUE, TRUE, 0 );

    Gtk::ScrolledWindow *attr_scroller = new Gtk::ScrolledWindow();
    attr_scroller->set_policy( Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC );
    attr_scroller->set_size_request(0, 60);

    attr_subpaned_container.pack1( *attr_scroller );
    attr_scroller->add(*Gtk::manage(Glib::wrap(GTK_WIDGET(attributes))));

    attr_vbox.pack_start( attr_hbox, FALSE, TRUE, 0);

    attr_name.set_tooltip_text(_("Attribute name") ); // TRANSLATORS: "Attribute" is a noun here
    attr_name.set_width_chars (10);
    attr_hbox.pack_start( attr_name, TRUE, TRUE, 0);

    set_attr.set_sensitive(FALSE);
    attr_hbox.pack_start(set_attr, FALSE, FALSE, 0);

    Gtk::ScrolledWindow *scroller = new Gtk::ScrolledWindow();
    scroller->set_policy( Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC );
    scroller->set_shadow_type(Gtk::SHADOW_IN);

    attr_vbox.pack_start(*scroller, TRUE, TRUE, 0);

    attr_value.set_size_request(0, 60);
    attr_value.set_wrap_mode(Gtk::WRAP_CHAR);
    attr_value.set_tooltip_text( _("Attribute value") );// TRANSLATORS: "Attribute" is a noun here
    attr_value.set_editable(TRUE);
    scroller->add(attr_value);

    attr_subpaned_container.pack2( attr_vbox, FALSE, TRUE );

    /* text */
    text_container.set_policy( Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC );
    right_box.pack_start(text_container, TRUE, TRUE, 0);

    content = SP_XMLVIEW_CONTENT(sp_xmlview_content_new(NULL));
    text_container.add(*Gtk::manage(Glib::wrap(GTK_WIDGET(content))));

    /* Signal handlers */
    g_signal_connect( G_OBJECT(tree), "tree_select_row", G_CALLBACK(on_tree_select_row), this );
    g_signal_connect( G_OBJECT(tree), "tree_unselect_row", G_CALLBACK(on_tree_unselect_row), this);
    g_signal_connect_after( G_OBJECT(tree), "tree_move", G_CALLBACK(after_tree_move), this);

    g_signal_connect( G_OBJECT(attributes), "select_row", G_CALLBACK(on_attr_select_row), this);
    g_signal_connect( G_OBJECT(attributes), "unselect_row", G_CALLBACK(on_attr_unselect_row), this);
    g_signal_connect( G_OBJECT(attributes), "row-value-changed", G_CALLBACK(on_attr_row_changed), this);

    xml_element_new_button.signal_clicked().connect(sigc::mem_fun(*this, &XmlTree::cmd_new_element_node));
    xml_text_new_button.signal_clicked().connect(sigc::mem_fun(*this, &XmlTree::cmd_new_text_node));
    xml_node_duplicate_button.signal_clicked().connect(sigc::mem_fun(*this, &XmlTree::cmd_duplicate_node));
    xml_node_delete_button.signal_clicked().connect(sigc::mem_fun(*this, &XmlTree::cmd_delete_node));
    unindent_node_button.signal_clicked().connect(sigc::mem_fun(*this, &XmlTree::cmd_unindent_node));
    indent_node_button.signal_clicked().connect(sigc::mem_fun(*this, &XmlTree::cmd_indent_node));
    raise_node_button.signal_clicked().connect(sigc::mem_fun(*this, &XmlTree::cmd_raise_node));
    lower_node_button.signal_clicked().connect(sigc::mem_fun(*this, &XmlTree::cmd_lower_node));
    xml_attribute_delete_button.signal_clicked().connect(sigc::mem_fun(*this, &XmlTree::cmd_delete_attr));
    set_attr.signal_clicked().connect(sigc::mem_fun(*this, &XmlTree::cmd_set_attr));
    attr_name.signal_changed().connect(sigc::mem_fun(*this, &XmlTree::onNameChanged));
    attr_value.signal_key_press_event().connect(sigc::mem_fun(*this, &XmlTree::sp_xml_tree_key_press), false);

    desktopChangeConn = deskTrack.connectDesktopChanged( sigc::mem_fun(*this, &XmlTree::set_tree_desktop) );
    deskTrack.connect(GTK_WIDGET(gobj()));

    /* initial show/hide */
    show_all();

/*
    // hide() doesn't seem to work in the constructor, so moved this to present()
    text_container.hide();
    attr_container.hide();
*/

    tree_reset_context();

    g_assert(desktop != NULL);
    set_tree_desktop(desktop);

}

void XmlTree::present()
{
    text_container.hide();
    attr_container.hide();

    set_tree_select(get_dt_select());

    UI::Widget::Panel::present();
}

XmlTree::~XmlTree (void)
{
    set_tree_desktop(NULL);

    _message_changed_connection.disconnect();
    delete _message_context;
    _message_context = NULL;
    Inkscape::GC::release(_message_stack);
    _message_stack = NULL;
    _message_changed_connection.~connection();

    //status = "";
}

void XmlTree::setDesktop(SPDesktop *desktop)
{
    Panel::setDesktop(desktop);
    deskTrack.setBase(desktop);
}

/**
 * Sets the XML status bar when the tree is selected.
 */
void XmlTree::tree_reset_context()
{
    _message_context->set(Inkscape::NORMAL_MESSAGE,
                          _("<b>Click</b> to select nodes, <b>drag</b> to rearrange."));
}


/**
 * Sets the XML status bar, depending on which attr is selected.
 */
void XmlTree::attr_reset_context(gint attr)
{
    if (attr == 0) {
        _message_context->set(Inkscape::NORMAL_MESSAGE,
                              _("<b>Click</b> attribute to edit."));
    }
    else {
        const gchar *name = g_quark_to_string(attr);
        gchar *message = g_strdup_printf(_("Attribute <b>%s</b> selected. Press <b>Ctrl+Enter</b> when done editing to commit changes."), name);
        _message_context->set(Inkscape::NORMAL_MESSAGE, message);
        g_free(message);
    }
}

bool XmlTree::sp_xml_tree_key_press(GdkEventKey *event)
{
    unsigned int shortcut = get_group0_keyval (event) |
        ( event->state & GDK_SHIFT_MASK ?
          SP_SHORTCUT_SHIFT_MASK : 0 ) |
        ( event->state & GDK_CONTROL_MASK ?
          SP_SHORTCUT_CONTROL_MASK : 0 ) |
        ( event->state & GDK_MOD1_MASK ?
          SP_SHORTCUT_ALT_MASK : 0 );

    /* fixme: if you need to add more xml-tree-specific callbacks, you should probably upgrade
     * the sp_shortcut mechanism to take into account windows. */
    if (shortcut == (SP_SHORTCUT_CONTROL_MASK | GDK_Return)) {
        cmd_set_attr();
        return true;
    }
    return false;
}

void XmlTree::set_tree_desktop(SPDesktop *desktop)
{
    if ( desktop == current_desktop ) {
        return;
    }

    if (current_desktop) {
        sel_changed_connection.disconnect();
        document_replaced_connection.disconnect();
    }
    current_desktop = desktop;
    if (desktop) {
        sel_changed_connection = sp_desktop_selection(desktop)->connectChanged(sigc::hide(sigc::mem_fun(this, &XmlTree::on_desktop_selection_changed)));
        document_replaced_connection = desktop->connectDocumentReplaced(sigc::mem_fun(this, &XmlTree::on_document_replaced));

        set_tree_document(sp_desktop_document(desktop));
    } else {
        set_tree_document(NULL);
    }

} // end of set_tree_desktop()


void XmlTree::set_tree_document(SPDocument *document)
{
    if (document == current_document) {
        return;
    }

    if (current_document) {
        document_uri_set_connection.disconnect();
    }
    current_document = document;
    if (current_document) {

        document_uri_set_connection = current_document->connectURISet(sigc::bind(sigc::ptr_fun(&on_document_uri_set), current_document));
        on_document_uri_set( current_document->getURI(), current_document );
        set_tree_repr(current_document->getReprRoot());
    } else {
        set_tree_repr(NULL);
    }
}



void XmlTree::set_tree_repr(Inkscape::XML::Node *repr)
{
    if (repr == selected_repr) {
        return;
    }

    gtk_clist_freeze(GTK_CLIST(tree));

    sp_xmlview_tree_set_repr(tree, repr);
    if (repr) {
        set_tree_select(get_dt_select());
    } else {
        set_tree_select(NULL);
    }

    gtk_clist_thaw(GTK_CLIST(tree));

    propagate_tree_select(selected_repr);

}



void XmlTree::set_tree_select(Inkscape::XML::Node *repr)
{
    if (selected_repr) {
        Inkscape::GC::release(selected_repr);
    }

    selected_repr = repr;
    if (repr) {
        GtkCTreeNode *node;

        Inkscape::GC::anchor(selected_repr);

        node = sp_xmlview_tree_get_repr_node(SP_XMLVIEW_TREE(tree), repr);
        if (node) {
            GtkCTreeNode *parent;

            gtk_ctree_select(GTK_CTREE(tree), node);

            parent = GTK_CTREE_ROW(node)->parent;
            while (parent) {
                gtk_ctree_expand(GTK_CTREE(tree), parent);
                parent = GTK_CTREE_ROW(parent)->parent;
            }

            gtk_ctree_node_moveto(GTK_CTREE(tree), node, 0, 0.66, 0.0);
        }
    } else {
        gtk_clist_unselect_all(GTK_CLIST(tree));
        on_tree_unselect_row_disable();
        on_tree_unselect_row_hide();
    }
    propagate_tree_select(repr);
}



void XmlTree::propagate_tree_select(Inkscape::XML::Node *repr)
{
    if (repr && repr->type() == Inkscape::XML::ELEMENT_NODE) {
        sp_xmlview_attr_list_set_repr(attributes, repr);
    } else {
        sp_xmlview_attr_list_set_repr(attributes, NULL);
    }

    if (repr && ( repr->type() == Inkscape::XML::TEXT_NODE || repr->type() == Inkscape::XML::COMMENT_NODE || repr->type() == Inkscape::XML::PI_NODE ) ) {
        sp_xmlview_content_set_repr(content, repr);
    } else {
        sp_xmlview_content_set_repr(content, NULL);
    }
}


Inkscape::XML::Node *XmlTree::get_dt_select()
{
    if (!current_desktop) {
        return NULL;
    }
    return sp_desktop_selection(current_desktop)->singleRepr();
}



void XmlTree::set_dt_select(Inkscape::XML::Node *repr)
{
    if (!current_desktop) {
        return;
    }

    Inkscape::Selection *selection = sp_desktop_selection(current_desktop);

    SPObject *object;
    if (repr) {
        while ( ( repr->type() != Inkscape::XML::ELEMENT_NODE )
                && repr->parent() )
        {
            repr = repr->parent();
        } // end of while loop

        object = sp_desktop_document(current_desktop)->getObjectByRepr(repr);
    } else {
        object = NULL;
    }

    blocked++;
    if ( object && in_dt_coordsys(*object)
         && !(SP_IS_STRING(object) ||
                SP_IS_ROOT(object)     ) )
    {
            /* We cannot set selection to root or string - they are not items and selection is not
             * equipped to deal with them */
            selection->set(SP_ITEM(object));
    }
    blocked--;

} // end of set_dt_select()


void XmlTree::on_tree_select_row(GtkCTree *tree,
                        GtkCTreeNode *node,
                        gint column,
                        gpointer data)
{
    XmlTree *self = (XmlTree *)data;

    Inkscape::XML::Node *repr = sp_xmlview_tree_node_get_repr(SP_XMLVIEW_TREE(tree), node);
    g_assert(repr != NULL);

    if (self->selected_repr) {
        Inkscape::GC::release(self->selected_repr);
        self->selected_repr = NULL;
    }
    self->selected_repr = repr;
    Inkscape::GC::anchor(self->selected_repr);

    self->propagate_tree_select(self->selected_repr);

    self->set_dt_select(self->selected_repr);

    self->tree_reset_context();

    self->on_tree_select_row_enable(node);
}

void XmlTree::on_tree_unselect_row(GtkCTree *tree,
                          GtkCTreeNode *node,
                          gint column,
                          gpointer data)
{
    XmlTree *self = (XmlTree *)data;

    if (self->blocked) {
        return;
    }


    Inkscape::XML::Node *repr = sp_xmlview_tree_node_get_repr(SP_XMLVIEW_TREE(tree), node);
    self->propagate_tree_select(NULL);
    self->set_dt_select(NULL);

    if (self->selected_repr && (self->selected_repr == repr)) {
        Inkscape::GC::release(self->selected_repr);
        self->selected_repr = NULL;
        self->selected_attr = 0;
    }

    self->on_tree_unselect_row_disable();
    self->on_tree_unselect_row_hide();
    self->on_attr_unselect_row_clear_text();
}



void XmlTree::after_tree_move(GtkCTree */*tree*/,
                     GtkCTreeNode *node,
                     GtkCTreeNode *new_parent,
                     GtkCTreeNode *new_sibling,
                     gpointer data)
{
    XmlTree *self = (XmlTree *)data;

    if (GTK_CTREE_ROW(node)->parent  == new_parent &&
        GTK_CTREE_ROW(node)->sibling == new_sibling)
    {
        DocumentUndo::done(self->current_document, SP_VERB_DIALOG_XML_EDITOR,
                           _("Drag XML subtree"));
    } else {
        DocumentUndo::cancel(self->current_document);
    }
}

void XmlTree::_set_status_message(Inkscape::MessageType /*type*/, const gchar *message, GtkWidget *widget)
{
    if (widget) {
        gtk_label_set_markup(GTK_LABEL(widget), message ? message : "");
    }
}

void XmlTree::on_tree_select_row_enable(GtkCTreeNode *node)
{
    if (!node) {
        return;
    }

    Inkscape::XML::Node *repr = sp_xmlview_tree_node_get_repr(SP_XMLVIEW_TREE(tree), node);
    Inkscape::XML::Node *parent=repr->parent();

    //on_tree_select_row_enable_if_mutable
    xml_node_duplicate_button.set_sensitive(xml_tree_node_mutable(node));
    xml_node_delete_button.set_sensitive(xml_tree_node_mutable(node));

    //on_tree_select_row_enable_if_element
    if (repr->type() == Inkscape::XML::ELEMENT_NODE) {
        xml_element_new_button.set_sensitive(true);
        xml_text_new_button.set_sensitive(true);

    } else {
        xml_element_new_button.set_sensitive(false);
        xml_text_new_button.set_sensitive(false);
    }

    //on_tree_select_row_enable_if_has_grandparent
    {
        GtkCTreeNode *parent = GTK_CTREE_ROW(node)->parent;

        if (parent) {
            GtkCTreeNode *grandparent = GTK_CTREE_ROW(parent)->parent;
            if (grandparent) {
                unindent_node_button.set_sensitive(true);
            } else {
                unindent_node_button.set_sensitive(false);
            }
        } else {
            unindent_node_button.set_sensitive(false);
        }
    }
    // on_tree_select_row_enable_if_indentable
    gboolean indentable = FALSE;

    if (xml_tree_node_mutable(node)) {
        Inkscape::XML::Node *prev;

        if ( parent && repr != parent->firstChild() ) {
            g_assert(parent->firstChild());

            // skip to the child just before the current repr
            for ( prev = parent->firstChild() ;
                  prev && prev->next() != repr ;
                  prev = prev->next() ){};

            if (prev && prev->type() == Inkscape::XML::ELEMENT_NODE) {
                indentable = TRUE;
            }
        }
    }

    indent_node_button.set_sensitive(indentable);

    //on_tree_select_row_enable_if_not_first_child
    {
        if ( parent && repr != parent->firstChild() ) {
            raise_node_button.set_sensitive(true);
        } else {
            raise_node_button.set_sensitive(false);
        }
    }

    //on_tree_select_row_enable_if_not_last_child
    {
        if ( parent && parent->parent() && repr->next() ) {
            lower_node_button.set_sensitive(true);
        } else {
            lower_node_button.set_sensitive(false);
        }
    }

    //on_tree_select_row_show_if_element
    if (repr->type() == Inkscape::XML::ELEMENT_NODE) {
        attr_container.show();
    } else {
        attr_container.hide();
    }

    //on_tree_select_row_show_if_text
    if ( repr->type() == Inkscape::XML::TEXT_NODE || repr->type() == Inkscape::XML::COMMENT_NODE || repr->type() == Inkscape::XML::PI_NODE ) {
        text_container.show();
    } else {
        text_container.hide();
    }

}


gboolean XmlTree::xml_tree_node_mutable(GtkCTreeNode *node)
{
    // top-level is immutable, obviously
    if (!GTK_CTREE_ROW(node)->parent) {
        return false;
    }

    // if not in base level (where namedview, defs, etc go), we're mutable
    if (GTK_CTREE_ROW(GTK_CTREE_ROW(node)->parent)->parent) {
        return true;
    }

    Inkscape::XML::Node *repr;
    repr = sp_xmlview_tree_node_get_repr(SP_XMLVIEW_TREE(tree), node);
    g_assert(repr);

    // don't let "defs" or "namedview" disappear
    if ( !strcmp(repr->name(),"svg:defs") ||
         !strcmp(repr->name(),"sodipodi:namedview") ) {
        return false;
    }

    // everyone else is okay, I guess.  :)
    return true;
}



void XmlTree::on_tree_unselect_row_disable()
{
    xml_text_new_button.set_sensitive(false);
    xml_element_new_button.set_sensitive(false);
    xml_node_delete_button.set_sensitive(false);
    xml_node_duplicate_button.set_sensitive(false);
    unindent_node_button.set_sensitive(false);
    indent_node_button.set_sensitive(false);
    raise_node_button.set_sensitive(false);
    lower_node_button.set_sensitive(false);
    xml_attribute_delete_button.set_sensitive(false);
}

void XmlTree::on_tree_unselect_row_hide()
{
    attr_container.hide();
    text_container.hide();
}

void XmlTree::on_attr_select_row(GtkCList *list, gint row, gint column,
                        GdkEventButton *event, gpointer data)
{
    XmlTree *self = (XmlTree *)data;

    self->selected_attr = sp_xmlview_attr_list_get_row_key(list, row);
    self->attr_value.grab_focus ();

    self->attr_reset_context(self->selected_attr);

    self->on_attr_select_row_enable();
    self->on_attr_select_row_set_name_content(row);
    self->on_attr_select_row_set_value_content(row);
}


void XmlTree::on_attr_unselect_row(GtkCList */*list*/, gint /*row*/, gint /*column*/,
                          GdkEventButton */*event*/, gpointer data)
{
    XmlTree *self = (XmlTree *)data;

    self->selected_attr = 0;
    self->attr_reset_context(self->selected_attr);

    self->on_attr_unselect_row_disable();
    self->on_attr_unselect_row_clear_text();
}


void XmlTree::on_attr_row_changed(GtkCList *list, gint row, gpointer data)
{
    gint attr = sp_xmlview_attr_list_get_row_key(list, row);

    XmlTree *self = (XmlTree *)data;
    if (attr == self->selected_attr) {
        /* if the attr changed, reselect the row in the list to sync
           the edit box */

        /*
        // get current attr values
        const gchar * name = g_quark_to_string (sp_xmlview_attr_list_get_row_key (list, row));
        const gchar * value = self->selected_repr->attribute(name);

        g_warning("value: '%s'",value);

        // get the edit box value
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds ( gtk_text_view_get_buffer (self->attr_value),
                                     &start, &end );
        gchar * text = gtk_text_buffer_get_text ( gtk_text_view_get_buffer (self->attr_value),
                                       &start, &end, TRUE );
        g_warning("text: '%s'",text);

        // compare to edit box
        if (strcmp(text,value)) {
            // issue warning if they're different
            _message_stack->flash(Inkscape::WARNING_MESSAGE,
                                  _("Attribute changed in GUI while editing values!"));
        }
        g_free (text);

        */
        gtk_clist_unselect_row( GTK_CLIST(list), row, 0 );
        gtk_clist_select_row( GTK_CLIST(list), row, 0 );
    }
}


void XmlTree::on_attr_select_row_set_name_content(gint row)
{
    const gchar *name = g_quark_to_string(sp_xmlview_attr_list_get_row_key(GTK_CLIST(attributes), row));
    attr_name.set_text(name);
}


void XmlTree::on_attr_select_row_set_value_content(gint row)
{
    const gchar *name = g_quark_to_string(sp_xmlview_attr_list_get_row_key(GTK_CLIST(attributes), row));
    const gchar *value = selected_repr->attribute(name);
    if (!value) {
        value = "";
    }
    attr_value.get_buffer()->set_text(value);
}

void XmlTree::on_attr_select_row_enable()
{
    xml_attribute_delete_button.set_sensitive(true);
}



void XmlTree::on_attr_unselect_row_disable()
{
    xml_attribute_delete_button.set_sensitive(false);
}


void XmlTree::on_attr_unselect_row_clear_text()
{
    attr_name.set_text("");
    attr_value.get_buffer()->set_text("", 0);
}

void XmlTree::onNameChanged()
{
    Glib::ustring text = attr_name.get_text();
    /* TODO: need to do checking a little more rigorous than this */
    if (!text.empty()) {
        set_attr.set_sensitive(true);
    } else {
        set_attr.set_sensitive(false);
    }
}

void XmlTree::onCreateNameChanged()
{
    Glib::ustring text = name_entry->get_text();
    /* TODO: need to do checking a little more rigorous than this */
    if (!text.empty()) {
        create_button->set_sensitive(true);
    } else {
        create_button->set_sensitive(false);
    }
}

void XmlTree::on_desktop_selection_changed()
{
    if (!blocked++) {
        Inkscape::XML::Node *node = get_dt_select();
        set_tree_select(node);
        if (!node) {
            on_attr_unselect_row_clear_text();
        }
    }
    blocked--;
}

void XmlTree::on_document_replaced(SPDesktop *dt, SPDocument *doc)
{
    if (current_desktop)
        sel_changed_connection.disconnect();

    sel_changed_connection = sp_desktop_selection(dt)->connectChanged(sigc::hide(sigc::mem_fun(this, &XmlTree::on_desktop_selection_changed)));
    set_tree_document(doc);
}

void XmlTree::on_document_uri_set(gchar const */*uri*/, SPDocument *document)
{
/*
 * Seems to be no way to set the title on a docked dialog
    gchar title[500];
    sp_ui_dialog_title_string(Inkscape::Verb::get(SP_VERB_DIALOG_XML_EDITOR), title);
    gchar *t = g_strdup_printf("%s: %s", document->getName(), title);
    //gtk_window_set_title(GTK_WINDOW(dlg), t);
    g_free(t);
*/
}

gboolean XmlTree::quit_on_esc (GtkWidget *w, GdkEventKey *event, GObject */*tbl*/)
{
    switch (get_group0_keyval (event)) {
        case GDK_Escape: // defocus
            gtk_widget_destroy(w);
            return TRUE;
    }
    return FALSE;
}

void XmlTree::cmd_new_element_node()
{
    GtkWidget *cancel, *vbox, *bbox, *sep;

    g_assert(selected_repr != NULL);

    new_window = sp_window_new(NULL, TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(new_window), 4);
    gtk_window_set_title(GTK_WINDOW(new_window), _("New element node..."));
    gtk_window_set_resizable(GTK_WINDOW(new_window), FALSE);
    gtk_window_set_position(GTK_WINDOW(new_window), GTK_WIN_POS_CENTER);
    gtk_window_set_transient_for(GTK_WINDOW(new_window), GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(gobj()))));
    gtk_window_set_modal(GTK_WINDOW(new_window), TRUE);
    g_signal_connect(G_OBJECT(new_window), "destroy", gtk_main_quit, NULL);
    g_signal_connect(G_OBJECT(new_window), "key-press-event", G_CALLBACK(quit_on_esc), new_window);

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_container_add(GTK_CONTAINER(new_window), vbox);

    name_entry = new Gtk::Entry();
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(name_entry->gobj()), FALSE, TRUE, 0);

    sep = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, TRUE, 0);

    bbox = gtk_hbutton_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(bbox), 4);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, TRUE, 0);

    cancel = gtk_button_new_with_label(_("Cancel"));
    g_signal_connect_swapped( G_OBJECT(cancel), "clicked",
                                G_CALLBACK(gtk_widget_destroy),
                                G_OBJECT(new_window) );
    gtk_container_add(GTK_CONTAINER(bbox), cancel);

    create_button = new Gtk::Button(_("Create"));
    create_button->set_sensitive(FALSE);

    name_entry->signal_changed().connect(sigc::mem_fun(*this, &XmlTree::onCreateNameChanged));

    g_signal_connect_swapped( G_OBJECT(create_button->gobj()), "clicked",
                                G_CALLBACK(gtk_widget_destroy),
                                G_OBJECT(new_window) );
    create_button->set_can_default( TRUE );
    create_button->set_receives_default( TRUE );
    gtk_container_add(GTK_CONTAINER(bbox), GTK_WIDGET(create_button->gobj()));

    gtk_widget_show_all(GTK_WIDGET(new_window));
    //gtk_window_set_default(GTK_WINDOW(window), GTK_WIDGET(create));
    name_entry->grab_focus();

    gtk_main();

    gchar *new_name = g_strdup(name_entry->get_text().c_str());

    if (selected_repr != NULL && new_name) {
        Inkscape::XML::Document *xml_doc = current_document->getReprDoc();
        Inkscape::XML::Node *new_repr;
        new_repr = xml_doc->createElement(new_name);
        Inkscape::GC::release(new_repr);
        g_free(new_name);
        selected_repr->appendChild(new_repr);
        set_tree_select(new_repr);
        set_dt_select(new_repr);

        DocumentUndo::done(current_document, SP_VERB_DIALOG_XML_EDITOR,
                           _("Create new element node"));
    }

} // end of cmd_new_element_node()



void XmlTree::cmd_new_text_node()
{
    g_assert(selected_repr != NULL);

    Inkscape::XML::Document *xml_doc = current_document->getReprDoc();
    Inkscape::XML::Node *text = xml_doc->createTextNode("");
    selected_repr->appendChild(text);

    DocumentUndo::done(current_document, SP_VERB_DIALOG_XML_EDITOR,
                       _("Create new text node"));

    set_tree_select(text);
    set_dt_select(text);

    gtk_window_set_focus(GTK_WINDOW(new_window), GTK_WIDGET(content));

}

void XmlTree::cmd_duplicate_node()
{
    g_assert(selected_repr != NULL);

    Inkscape::XML::Node *parent = selected_repr->parent();
    Inkscape::XML::Node *dup = selected_repr->duplicate(parent->document());
    parent->addChild(dup, selected_repr);

    DocumentUndo::done(current_document, SP_VERB_DIALOG_XML_EDITOR,
                       _("Duplicate node"));

    GtkCTreeNode *node = sp_xmlview_tree_get_repr_node(SP_XMLVIEW_TREE(tree), dup);

    if (node) {
        gtk_ctree_select(GTK_CTREE(tree), node);
    }
}

void XmlTree::cmd_delete_node()
{
    g_assert(selected_repr != NULL);
    sp_repr_unparent(selected_repr);

    DocumentUndo::done(current_document, SP_VERB_DIALOG_XML_EDITOR,
                       Q_("nodeAsInXMLinHistoryDialog|Delete node"));
}



void XmlTree::cmd_delete_attr()
{
    g_assert(selected_repr != NULL);
    g_assert(selected_attr != 0);

    selected_repr->setAttribute(g_quark_to_string(selected_attr), NULL);

    SPObject *updated = current_document->getObjectByRepr(selected_repr);
    if (updated) {
        // force immediate update of dependant attributes
        updated->updateRepr();
    }

    DocumentUndo::done(current_document, SP_VERB_DIALOG_XML_EDITOR,
                       _("Delete attribute"));
}



void XmlTree::cmd_set_attr()
{
    g_assert(selected_repr != NULL);

    gchar *name = g_strdup(attr_name.get_text().c_str());
    gchar *value = g_strdup(attr_value.get_buffer()->get_text().c_str());

    selected_repr->setAttribute(name, value, false);

    g_free(name);
    g_free(value);

    SPObject *updated = current_document->getObjectByRepr(selected_repr);
    if (updated) {
        // force immediate update of dependant attributes
        updated->updateRepr();
    }

    DocumentUndo::done(current_document, SP_VERB_DIALOG_XML_EDITOR,
                       _("Change attribute"));

    /* TODO: actually, the row won't have been created yet.  why? */
    gint row = sp_xmlview_attr_list_find_row_from_key(GTK_CLIST(attributes),
                                                      g_quark_from_string(name));
    if (row != -1) {
        gtk_clist_select_row(GTK_CLIST(attributes), row, 0);
    }
}


void XmlTree::cmd_raise_node()
{
    g_assert(selected_repr != NULL);


    Inkscape::XML::Node *parent = selected_repr->parent();
    g_return_if_fail(parent != NULL);
    g_return_if_fail(parent->firstChild() != selected_repr);

    Inkscape::XML::Node *ref = NULL;
    Inkscape::XML::Node *before = parent->firstChild();
    while (before && before->next() != selected_repr) {
        ref = before;
        before = before->next();
    }

    parent->changeOrder(selected_repr, ref);

    DocumentUndo::done(current_document, SP_VERB_DIALOG_XML_EDITOR,
                       _("Raise node"));

    set_tree_select(selected_repr);
    set_dt_select(selected_repr);
}



void XmlTree::cmd_lower_node()
{
    g_assert(selected_repr != NULL);

    g_return_if_fail(selected_repr->next() != NULL);
    Inkscape::XML::Node *parent = selected_repr->parent();

    parent->changeOrder(selected_repr, selected_repr->next());

    DocumentUndo::done(current_document, SP_VERB_DIALOG_XML_EDITOR,
                       _("Lower node"));

    set_tree_select(selected_repr);
    set_dt_select(selected_repr);
}

void XmlTree::cmd_indent_node()
{
    Inkscape::XML::Node *repr = selected_repr;
    g_assert(repr != NULL);

    Inkscape::XML::Node *parent = repr->parent();
    g_return_if_fail(parent != NULL);
    g_return_if_fail(parent->firstChild() != repr);

    Inkscape::XML::Node* prev = parent->firstChild();
    while (prev && prev->next() != repr) {
        prev = prev->next();
    }
    g_return_if_fail(prev != NULL);
    g_return_if_fail(prev->type() == Inkscape::XML::ELEMENT_NODE);

    Inkscape::XML::Node* ref = NULL;
    if (prev->firstChild()) {
        for( ref = prev->firstChild() ; ref->next() ; ref = ref->next() ){};
    }

    parent->removeChild(repr);
    prev->addChild(repr, ref);

    DocumentUndo::done(current_document, SP_VERB_DIALOG_XML_EDITOR,
                       _("Indent node"));
    set_tree_select(repr);
    set_dt_select(repr);

} // end of cmd_indent_node()



void XmlTree::cmd_unindent_node()
{
    Inkscape::XML::Node *repr = selected_repr;
    g_assert(repr != NULL);

    Inkscape::XML::Node *parent = repr->parent();
    g_return_if_fail(parent);
    Inkscape::XML::Node *grandparent = parent->parent();
    g_return_if_fail(grandparent);

    parent->removeChild(repr);
    grandparent->addChild(repr, parent);

    DocumentUndo::done(current_document, SP_VERB_DIALOG_XML_EDITOR,
                       _("Unindent node"));
    set_tree_select(repr);
    set_dt_select(repr);

} // end of cmd_unindent_node()

/** Returns true iff \a item is suitable to be included in the selection, in particular
    whether it has a bounding box in the desktop coordinate system for rendering resize handles.

    Descendents of <defs> nodes (markers etc.) return false, for example.
*/
bool XmlTree::in_dt_coordsys(SPObject const &item)
{
    /* Definition based on sp_item_i2doc_affine. */
    SPObject const *child = &item;
    g_return_val_if_fail(child != NULL, false);
    for(;;) {
        if (!SP_IS_ITEM(child)) {
            return false;
        }
        SPObject const * const parent = child->parent;
        if (parent == NULL) {
            break;
        }
        child = parent;
    }
    g_assert(SP_IS_ROOT(child));
    /* Relevance: Otherwise, I'm not sure whether to return true or false. */
    return true;
}

}
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