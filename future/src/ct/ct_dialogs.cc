/*
 * ct_dialogs.cc
 *
 * Copyright 2017-2019 Giuseppe Penone <giuspen@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "ct_dialogs.h"
#include <gtkmm/stock.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treeview.h>
#include <gtkmm/cellrendererpixbuf.h>
#include <gtkmm/colorchooserdialog.h>
#include "ct_app.h"
#include "ct_treestore.h"

CtDialogTextEntry::CtDialogTextEntry(const Glib::ustring& title,
                                     const bool forPassword,
                                     Gtk::Window* pParent)
{
    set_title(title);
    set_transient_for(*pParent);
    set_modal();

    add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
    add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    set_default_response(Gtk::RESPONSE_OK);

    _entry.set_icon_from_stock(Gtk::Stock::CLEAR, Gtk::ENTRY_ICON_SECONDARY);
    _entry.set_size_request(350, -1);
    if (forPassword)
    {
        _entry.set_visibility(false);
    }
    get_vbox()->pack_start(_entry, true, true, 0);

    // Special signals which correspond to the underlying X-Windows events are suffixed by _event.
    // By default, your signal handlers are called after any previously-connected signal handlers. However, this can be a problem with the X Event signals. For instance, the existing signal handlers, or the default signal handler, might return true to stop other signal handlers from being called. To specify that your signal handler should be called before the other signal handlers, so that it will always be called, you can specify false for the optional after parameter
    _entry.signal_key_press_event().connect(sigc::mem_fun(*this, &CtDialogTextEntry::_on_entry_key_press_event), false/*call me before other*/);

    _entry.signal_icon_press().connect(sigc::mem_fun(*this, &CtDialogTextEntry::_on_entry_icon_press));

    get_vbox()->show_all();
}

bool CtDialogTextEntry::_on_entry_key_press_event(GdkEventKey *pEventKey)
{
    if (GDK_KEY_Return == pEventKey->keyval)
    {
        Gtk::Button *pButton = static_cast<Gtk::Button*>(get_widget_for_response(Gtk::RESPONSE_OK));
        pButton->clicked();
        return true;
    }
    return false;
}


Gtk::TreeIter CtDialogs::choose_item_dialog(Gtk::Window& parent,
                                            const Glib::ustring& title,
                                            Glib::RefPtr<CtChooseDialogListStore> rModel,
                                            const gchar* single_column_name /* = nullptr */)
{
    Gtk::Dialog dialog(title,
                       parent,
                       Gtk::DialogFlags::DIALOG_MODAL | Gtk::DialogFlags::DIALOG_DESTROY_WITH_PARENT);
    dialog.set_transient_for(parent);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_REJECT);
    dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);
    dialog.set_default_response(Gtk::RESPONSE_ACCEPT);
    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
    dialog.set_default_size(400, 300);
    Gtk::ScrolledWindow* pScrolledwindow = Gtk::manage(new Gtk::ScrolledWindow());
    pScrolledwindow->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    Gtk::TreeView* pElementsTreeview = Gtk::manage(new Gtk::TreeView(rModel));
    pElementsTreeview->set_headers_visible(false);
    Gtk::CellRendererPixbuf pixbuf_renderer;
    if (nullptr == single_column_name)
    {
        int col_num = pElementsTreeview->append_column("", pixbuf_renderer) - 1;
        pElementsTreeview->get_column(col_num)->add_attribute(pixbuf_renderer, "icon-name", rModel->columns.stock_id);
        pElementsTreeview->append_column("", rModel->columns.desc);
    }
    else
    {
        pElementsTreeview->append_column(single_column_name, rModel->columns.desc);
    }
    pScrolledwindow->add(*pElementsTreeview);
    //list_parms->sel_iter = elements_liststore->get_iter_first()
    //if list_parms->sel_iter:
    //    pElementsTreeview->set_cursor(elements_liststore->get_path(list_parms->sel_iter))
    Gtk::Box* pContentArea = dialog.get_content_area();
    pContentArea->pack_start(*pScrolledwindow);
    pContentArea->show_all();
    pElementsTreeview->grab_focus();

    return (Gtk::RESPONSE_ACCEPT == dialog.run() ? pElementsTreeview->get_selection()->get_selected() : Gtk::TreeIter());
}

// Dialog to select a color, featuring a palette
bool CtDialogs::color_pick_dialog(Gtk::Window& parent,
                                  Gdk::RGBA& color)
{
    Gtk::ColorChooserDialog dialog(_("Pick a Color"),
                                   parent);
    dialog.set_transient_for(parent);
    dialog.set_modal(true);
    dialog.property_destroy_with_parent() = true;
    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
    std::vector<std::string> colors = str::split(CtApp::P_ctCfg->colorPalette, ":");
    std::vector<Gdk::RGBA> rgbas;
    for (const std::string& color : colors)
    {
        rgbas.push_back(Gdk::RGBA(color));
    }
    dialog.add_palette(Gtk::Orientation::ORIENTATION_HORIZONTAL, 10, rgbas);
    dialog.set_rgba(color);

    if (Gtk::RESPONSE_OK != dialog.run())
    {
        return false;
    }

    std::string ret_color_hex8 = CtRgbUtil::rgb_any_to_24(dialog.get_rgba());
    size_t color_qty = colors.size();
    colors.erase(std::find(colors.begin(), colors.end(), ret_color_hex8));
    if (color_qty == colors.size())
    {
        colors.pop_back();
    }
    colors.insert(colors.begin(), ret_color_hex8);

    color = dialog.get_rgba();
    return true;
}

// The Question dialog, returns True if the user presses OK
bool CtDialogs::question_dialog(const Glib::ustring& message,
                                Gtk::Window& parent)
{
    Gtk::MessageDialog dialog(parent,
                              _("Question"),
                              true/* use_markup */,
                              Gtk::MESSAGE_QUESTION,
                              Gtk::BUTTONS_OK_CANCEL,
                              true/* modal */);
    dialog.set_secondary_text(message);
    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
    return (Gtk::RESPONSE_OK == dialog.run());
}

// The Info dialog
void CtDialogs::info_dialog(const Glib::ustring& message,
                            Gtk::Window& parent)
{
    Gtk::MessageDialog dialog(parent,
                              _("Info"),
                              true/* use_markup */,
                              Gtk::MESSAGE_INFO,
                              Gtk::BUTTONS_OK,
                              true/* modal */);
    dialog.set_secondary_text(message);
    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
    dialog.run();
}

// The Warning dialog
void CtDialogs::warning_dialog(const Glib::ustring& message,
                               Gtk::Window& parent)
{
    Gtk::MessageDialog dialog(parent,
                              _("Warning"),
                              true/* use_markup */,
                              Gtk::MESSAGE_WARNING,
                              Gtk::BUTTONS_OK,
                              true/* modal */);
    dialog.set_secondary_text(message);
    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
    dialog.run();
}

// The Error dialog
void CtDialogs::error_dialog(const Glib::ustring& message,
                             Gtk::Window& parent)
{
    Gtk::MessageDialog dialog(parent,
                              _("Error"),
                              true/* use_markup */,
                              Gtk::MESSAGE_ERROR,
                              Gtk::BUTTONS_OK,
                              true/* modal */);
    dialog.set_secondary_text(message);
    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
    dialog.run();
}

// Dialog to Select a Node
Gtk::TreeIter CtDialogs::choose_node_dialog(Gtk::Window& parent,
                                            Gtk::TreeView& parentTreeView,
                                            const Glib::ustring& title,
                                            CtTreeStore* pCtTreeStore,
                                            Gtk::TreeIter sel_tree_iter)
{
    Gtk::Dialog dialog(title,
                       parent,
                       Gtk::DialogFlags::DIALOG_MODAL | Gtk::DialogFlags::DIALOG_DESTROY_WITH_PARENT);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_REJECT);
    dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);
    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
    dialog.set_default_size(600, 500);
    Gtk::TreeView treeview_2(pCtTreeStore->get_store());
    treeview_2.set_headers_visible(false);
    treeview_2.set_search_column(1);
    treeview_2.append_column("", pCtTreeStore->get_columns().rColPixbuf);
    treeview_2.append_column("", pCtTreeStore->get_columns().colNodeName);
    Gtk::ScrolledWindow scrolledwindow;
    scrolledwindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    scrolledwindow.add(treeview_2);
    Gtk::Box* pContentArea = dialog.get_content_area();
    pContentArea->pack_start(scrolledwindow);

    auto expand_collapse_row = [&treeview_2](Gtk::TreePath path)
    {
        if (treeview_2.row_expanded(path))
        {
            treeview_2.collapse_row(path);
        }
        else
        {
            treeview_2.expand_row(path, false);
        }
    };

    treeview_2.signal_event().connect([&treeview_2, &expand_collapse_row](GdkEvent* event)->bool
    {
        bool retVal{false}; // propagate event
        if ( (event->type != GDK_BUTTON_PRESS) &&
             (event->type != GDK_2BUTTON_PRESS) &&
             (event->type != GDK_KEY_PRESS) )
        {
            // do nothing
        }
        else if ( (event->type == GDK_BUTTON_PRESS) &&
             (event->button.button == 2) )
        {
            Gtk::TreePath path_at_click;
            if (treeview_2.get_path_at_pos((int)event->button.x, (int)event->button.y, path_at_click))
            {
                expand_collapse_row(path_at_click);
                retVal = true; // stop event
            }
        }
        else if ( (event->type == GDK_2BUTTON_PRESS) &&
                  (event->button.button == 1) )
        {
            if (treeview_2.get_selection()->get_selected())
            {
                expand_collapse_row(treeview_2.get_model()->get_path(treeview_2.get_selection()->get_selected()));
                retVal = true; // stop event
            }
        }
        else if ( (event->type == GDK_KEY_PRESS) &&
                  (treeview_2.get_selection()->get_selected()) )
        {
            if (event->key.keyval == GDK_KEY_Left)
            {
                treeview_2.collapse_row(treeview_2.get_model()->get_path(treeview_2.get_selection()->get_selected()));
                retVal = true; // stop event
            }
            else if (event->key.keyval == GDK_KEY_Right)
            {
                treeview_2.expand_row(treeview_2.get_model()->get_path(treeview_2.get_selection()->get_selected()), false);
                retVal = true; // stop event
            }
        }
        return retVal;
    });
    
    pContentArea->show_all();
    std::string expanded_collapsed_string = pCtTreeStore->get_tree_expanded_collapsed_string(parentTreeView);
    pCtTreeStore->set_tree_expanded_collapsed_string(expanded_collapsed_string, treeview_2, CtApp::P_ctCfg->nodesBookmExp);
    if (sel_tree_iter)
    {
        Gtk::TreePath sel_path = treeview_2.get_model()->get_path(sel_tree_iter);
        treeview_2.expand_to_path(sel_path);
        treeview_2.set_cursor(sel_path);
        treeview_2.scroll_to_row(sel_path);
    }

    return (dialog.run() == Gtk::RESPONSE_ACCEPT ? treeview_2.get_selection()->get_selected() : Gtk::TreeIter());
}

// Handle the Bookmarks List
void CtDialogs::bookmarks_handle_dialog(CtMainWin* pCtMainWin)
{
    CtTreeStore& ctTreestore = pCtMainWin->get_tree_store();
    const std::list<gint64>& bookmarks = ctTreestore.get_bookmarks();

    Gtk::Dialog dialog(_("Handle the Bookmarks List"),
                       *pCtMainWin,
                       Gtk::DialogFlags::DIALOG_MODAL | Gtk::DialogFlags::DIALOG_DESTROY_WITH_PARENT);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_REJECT);
    dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);
    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
    dialog.set_default_size(500, 400);

    Glib::RefPtr<CtChooseDialogTreeStore> rModel = CtChooseDialogTreeStore::create();
    for (const gint64& node_id : bookmarks)
    {
        rModel->add_row("pin", "", ctTreestore.get_node_name_from_node_id(node_id), node_id);
    }

    Gtk::TreeView treeview(rModel);
    treeview.set_headers_visible(false);
    treeview.set_reorderable(true);
    Gtk::CellRendererPixbuf pixbuf_renderer;
    int col_num = treeview.append_column("", pixbuf_renderer) - 1;
    treeview.get_column(col_num)->add_attribute(pixbuf_renderer, "icon-name", rModel->columns.stock_id);
    treeview.append_column("", rModel->columns.desc);
    Gtk::ScrolledWindow scrolledwindow;
    scrolledwindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    scrolledwindow.add(treeview);
    Gtk::Box* pContentArea = dialog.get_content_area();

    Gtk::Button button_move_up;
    button_move_up.set_image_from_icon_name("gtk-go-up", Gtk::ICON_SIZE_DND);
    Gtk::Button button_move_down;
    button_move_down.set_image_from_icon_name("gtk-go-down", Gtk::ICON_SIZE_DND);
    Gtk::Button button_delete;
    button_delete.set_image_from_icon_name("gtk-clear", Gtk::ICON_SIZE_DND);
    Gtk::Button button_sort_desc;
    button_sort_desc.set_image_from_icon_name("gtk-sort-descending", Gtk::ICON_SIZE_DND);
    Gtk::Button button_sort_asc;
    button_sort_asc.set_image_from_icon_name("gtk-sort-ascending", Gtk::ICON_SIZE_DND);
    Gtk::Label label1;
    Gtk::Label label2;
    Gtk::HBox hbox;
    Gtk::VBox vbox;
    vbox.set_spacing(1);
    vbox.pack_start(button_move_up, false, false);
    vbox.pack_start(button_move_down, false, false);
    vbox.pack_start(button_delete, false, false);
    vbox.pack_start(label1, true, true);
    vbox.pack_start(button_sort_desc, false, false);
    vbox.pack_start(button_sort_asc, false, false);
    vbox.pack_start(label2, true, false);
    hbox.pack_start(scrolledwindow, true, true);
    hbox.pack_start(vbox, false, false);
    pContentArea->pack_start(hbox);
    pContentArea->show_all();

    treeview.signal_key_press_event().connect([&rModel, &treeview](GdkEventKey* key)->bool
    {
        if (key->keyval == GDK_KEY_Delete)
        {
            Gtk::TreeIter tree_iter = treeview.get_selection()->get_selected();
            if (tree_iter)
            {
                rModel->erase(tree_iter);
            }
            return true; // stop event
        }
        return false; // propagate event
    });
    treeview.signal_button_press_event().connect([&treeview, &rModel, &pCtMainWin, &ctTreestore](GdkEventButton* event)->bool
    {
        if (event->button != 1 || event->type != GDK_2BUTTON_PRESS)
        {
            return false; // propagate event
        }
        Gtk::TreePath clicked_path;
        if (false == treeview.get_path_at_pos((int)event->x, (int)event->y, clicked_path))
        {
            return false; // propagate event
        }
        Gtk::TreeIter clicked_iter = rModel->get_iter(clicked_path);
        gint64 node_id = clicked_iter->get_value(rModel->columns.node_id);
        Gtk::TreeIter tree_iter = ctTreestore.get_node_from_node_id(node_id);
        pCtMainWin->get_tree_view().set_cursor_safe(tree_iter);
        return true; // stop event
    });
    button_move_up.signal_clicked().connect([&treeview, &rModel]()
    {
        Gtk::TreeIter curr_iter = treeview.get_selection()->get_selected();
        Gtk::TreeIter prev_iter = --treeview.get_selection()->get_selected();
        if (curr_iter && prev_iter)
        {
            rModel->iter_swap(curr_iter, prev_iter);
        }
    });
    button_move_down.signal_clicked().connect([&treeview, &rModel]()
    {
        Gtk::TreeIter curr_iter = treeview.get_selection()->get_selected();
        Gtk::TreeIter next_iter = ++treeview.get_selection()->get_selected();
        if (curr_iter && next_iter)
        {
            rModel->iter_swap(curr_iter, next_iter);
        }
    });
    button_delete.signal_clicked().connect([&treeview, &rModel]()
    {
        Gtk::TreeIter tree_iter = treeview.get_selection()->get_selected();
        if (tree_iter)
        {
            rModel->erase(tree_iter);
        }
    });
    button_sort_asc.signal_clicked().connect([&rModel]()
    {
        auto need_swap = [&rModel](Gtk::TreeIter& l, Gtk::TreeIter& r)
        {
            int cmp = l->get_value(rModel->columns.desc).compare(r->get_value(rModel->columns.desc));
            return (cmp > 0);
        };
        CtMiscUtil::node_siblings_sort_iteration(rModel, rModel->children(), need_swap);
    });
    button_sort_desc.signal_clicked().connect([&rModel]()
    {
        auto need_swap = [&rModel](Gtk::TreeIter& l, Gtk::TreeIter& r)
        {
            int cmp = l->get_value(rModel->columns.desc).compare(r->get_value(rModel->columns.desc));
            return (cmp < 0);
        };
        CtMiscUtil::node_siblings_sort_iteration(rModel, rModel->children(), need_swap);
    });

    if (dialog.run() != Gtk::RESPONSE_ACCEPT)
    {
        return;
    }

    std::set<gint64> temp_bookmarks;
    std::list<gint64> temp_bookmarks_order;
    rModel->foreach_iter([&temp_bookmarks, &temp_bookmarks_order, &rModel](const Gtk::TreeIter& iter)
    {
        gint64 node_id = iter->get_value(rModel->columns.node_id);
        temp_bookmarks.insert(node_id);
        temp_bookmarks_order.push_back(node_id);
        return false; /* to continue */
    });

    std::list<gint64> removed_bookmarks;
    for (const gint64& node_id : bookmarks)
    {
        if (0 == temp_bookmarks.count(node_id))
        {
            removed_bookmarks.push_back(node_id);
        }
    }

    ctTreestore.set_bookmarks(temp_bookmarks_order);
    gint64 curr_node_id = pCtMainWin->curr_tree_iter().get_node_id();
    for (gint64& node_id: removed_bookmarks)
    {
        Gtk::TreeIter tree_iter = ctTreestore.get_node_from_node_id(node_id);
        if (tree_iter)
        {
            ctTreestore.update_node_aux_icon(tree_iter);
            if (curr_node_id == node_id)
            {
                pCtMainWin->menu_tree_update_for_bookmarked_node(false);
            }
        }
    }

    pCtMainWin->set_bookmarks_menu_items();
    ctTreestore.pending_edit_db_bookmarks();
    pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::book);
}

// Dialog to select a Date
std::time_t CtDialogs::date_select_dialog(Gtk::Window& parent,
                                          const Glib::ustring& title,
                                          const std::time_t& curr_time)
{
    Gtk::Dialog dialog(title,
                       parent,
                       Gtk::DialogFlags::DIALOG_MODAL | Gtk::DialogFlags::DIALOG_DESTROY_WITH_PARENT);
    dialog.set_transient_for(parent);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_REJECT);
    dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);
    dialog.set_default_response(Gtk::RESPONSE_ACCEPT);
    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);

    std::tm struct_time = *std::localtime(&curr_time);

    Gtk::Box* pContentArea = dialog.get_content_area();
    Gtk::Calendar calendar;
    calendar.select_month((guint)(struct_time.tm_mon-1), (guint)struct_time.tm_year); // month 0-11
    calendar.select_day((guint)struct_time.tm_mday); // day 1-31
    Glib::RefPtr<Gtk::Adjustment> rAdj_h = Gtk::Adjustment::create(struct_time.tm_hour, 0, 23, 1);
    Gtk::SpinButton spinbutton_h(rAdj_h);
    spinbutton_h.set_value(struct_time.tm_hour);
    Glib::RefPtr<Gtk::Adjustment> rAdj_m = Gtk::Adjustment::create(struct_time.tm_min, 0, 59, 1);
    Gtk::SpinButton spinbutton_m(rAdj_m);
    spinbutton_m.set_value(struct_time.tm_min);
    Gtk::HBox hbox;
    hbox.pack_start(spinbutton_h);
    hbox.pack_start(spinbutton_m);
    pContentArea->pack_start(calendar);
    pContentArea->pack_start(hbox);

    dialog.signal_button_press_event().connect([&dialog](GdkEventButton* event)->bool
    {
        if ( (event->button == 1) &&
             (event->type == GDK_2BUTTON_PRESS) )
        {
            return false;
        }
        dialog.response(Gtk::RESPONSE_ACCEPT);
        return true;
    });
    pContentArea->show_all();

    if (dialog.run() != Gtk::RESPONSE_ACCEPT)
    {
        return 0;
    }

    guint new_year, new_month, new_day;
    calendar.get_date(new_year, new_month, new_day);

    std::tm tmtime = {};
    tmtime.tm_year = (int)new_year;
    tmtime.tm_mon = (int)(new_month) + 1;
    tmtime.tm_mday = (int)new_day;
    tmtime.tm_hour = spinbutton_h.get_value_as_int();
    tmtime.tm_min = spinbutton_m.get_value_as_int();

    std::time_t new_time = std::mktime(&tmtime);
    return new_time;
}

void CtDialogs::match_dialog(const Glib::ustring& title,
                             CtMainWin& ctMainWin,
                             Glib::RefPtr<CtMatchDialogStore> rModel)
{
    /* will be deleted on hide event */
    Gtk::Dialog* pAllMatchesDialog = new Gtk::Dialog(title,
                                                     ctMainWin,
                                                     Gtk::DialogFlags::DIALOG_DESTROY_WITH_PARENT);
    pAllMatchesDialog->set_transient_for(ctMainWin);
    if (rModel->dlg_size[0] > 0)
    {
        pAllMatchesDialog->set_default_size(rModel->dlg_size[0], rModel->dlg_size[1]);
        pAllMatchesDialog->move(rModel->dlg_pos[0], rModel->dlg_pos[1]);
    }
    else
    {
        pAllMatchesDialog->set_default_size(700, 350);
        pAllMatchesDialog->set_position(Gtk::WIN_POS_CENTER_ON_PARENT);
    }
    CtAction* pAction = ctMainWin.get_ct_menu().find_action("toggle_show_allmatches_dlg");
    Gtk::Button* pButtonHide = pAllMatchesDialog->add_button(str::format(_("Hide (Restore with '%s')"), pAction->get_shortcut()), Gtk::RESPONSE_CLOSE);
    pButtonHide->set_image_from_icon_name(Gtk::Stock::CLOSE.id, Gtk::ICON_SIZE_BUTTON);
    Gtk::TreeView* pTreeview = Gtk::manage(new Gtk::TreeView(rModel));
    pTreeview->append_column(_("Node Name"), rModel->columns.node_name);
    pTreeview->append_column(_("Line"), rModel->columns.line_num);
    pTreeview->append_column(_("Line Content"), rModel->columns.line_content);
    pTreeview->append_column("", rModel->columns.node_hier_name);
    pTreeview->get_column(3)->property_visible() = false;
    pTreeview->set_tooltip_column(3);
    Gtk::ScrolledWindow* pScrolledwindowAllmatches = Gtk::manage(new Gtk::ScrolledWindow());
    pScrolledwindowAllmatches->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    pScrolledwindowAllmatches->add(*pTreeview);
    Gtk::Box* pContentArea = pAllMatchesDialog->get_content_area();
    pContentArea->pack_start(*pScrolledwindowAllmatches);

    if (rModel->saved_path)
    {
        pTreeview->set_cursor(rModel->saved_path);
        pTreeview->scroll_to_row(rModel->saved_path, 0.5);
    }

    pTreeview->signal_event_after().connect([pTreeview, rModel, &ctMainWin](GdkEvent* event)
    {
        if ( (event->type != GDK_BUTTON_PRESS) &&
             (event->type != GDK_KEY_PRESS) )
        {
            return;
        }
        Gtk::TreeIter list_iter = pTreeview->get_selection()->get_selected();
        if (!list_iter)
        {
            return;
        }
        gint64 node_id = list_iter->get_value(rModel->columns.node_id);
        CtTreeIter tree_iter = ctMainWin.get_tree_store().get_node_from_node_id(node_id);
        if (!tree_iter)
        {
            CtDialogs::error_dialog(str::format(_("The Link Refers to a Node that Does Not Exist Anymore (Id = %s)"), node_id), ctMainWin);
            rModel->erase(list_iter);
            return;
        }
        ctMainWin.get_tree_view().set_cursor_safe(tree_iter);
        auto rCurrBuffer = ctMainWin.get_text_view().get_buffer();
        rCurrBuffer->move_mark(rCurrBuffer->get_insert(),
                               rCurrBuffer->get_iter_at_offset(list_iter->get_value(rModel->columns.start_offset)));
        rCurrBuffer->move_mark(rCurrBuffer->get_selection_bound(),
                               rCurrBuffer->get_iter_at_offset(list_iter->get_value(rModel->columns.end_offset)));
        ctMainWin.get_text_view().scroll_to(rCurrBuffer->get_insert(), CtTextView::TEXT_SCROLL_MARGIN);
    });
    pButtonHide->signal_clicked().connect([pAllMatchesDialog]()
    {
        pAllMatchesDialog->hide();
    });
    pAllMatchesDialog->signal_hide().connect([pAllMatchesDialog, pTreeview, rModel]()
    {
        pAllMatchesDialog->get_position(rModel->dlg_pos[0], rModel->dlg_pos[1]);
        pAllMatchesDialog->get_size(rModel->dlg_size[0], rModel->dlg_size[1]);
        Gtk::TreeIter list_iter = pTreeview->get_selection()->get_selected();
        rModel->saved_path = pTreeview->get_model()->get_path(list_iter);

        delete pAllMatchesDialog;
    });

    pAllMatchesDialog->show_all();
}

// Insert/Edit Anchor Name
Glib::ustring CtDialogs::img_n_entry_dialog(Gtk::Window& parent,
                                            const Glib::ustring& title,
                                            const Glib::ustring& entry_content,
                                            const char* img_stock)
{
    Gtk::Dialog dialog(title,
                       parent,
                       Gtk::DialogFlags::DIALOG_MODAL | Gtk::DialogFlags::DIALOG_DESTROY_WITH_PARENT);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_REJECT);
    dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);
    dialog.set_default_response(Gtk::RESPONSE_ACCEPT);
    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
    dialog.set_default_size(300, -1);
    Gtk::Image image;
    image.set_from_icon_name(img_stock, Gtk::ICON_SIZE_BUTTON);
    Gtk::Entry entry;
    entry.set_text(entry_content);
    Gtk::HBox hbox;
    hbox.pack_start(image, false, false);
    hbox.pack_start(entry);
    hbox.set_spacing(5);
    Gtk::Box* pContentArea = dialog.get_content_area();
    pContentArea->pack_start(hbox);
    pContentArea->show_all();
    entry.grab_focus();
    return (Gtk::RESPONSE_ACCEPT == dialog.run() ? str::trim(entry.get_text()) : "");
}

// Dialog to Insert/Edit Links
bool CtDialogs::link_handle_dialog(CtMainWin& ctMainWin,
                                   const Glib::ustring& title,
                                   Gtk::TreeIter sel_tree_iter,
                                   CtLinkEntry& link_entries)
{
    CtTreeStore& ctTreestore = ctMainWin.get_tree_store();
    Gtk::Dialog dialog(title,
                       ctMainWin,
                       Gtk::DialogFlags::DIALOG_MODAL | Gtk::DialogFlags::DIALOG_DESTROY_WITH_PARENT);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_REJECT);
    dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);
    dialog.set_default_response(Gtk::RESPONSE_ACCEPT);
    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
    dialog.set_default_size(600, 500);

    Gtk::HBox hbox_webs;
    Gtk::Image image_webs;
    image_webs.set_from_icon_name("link_website", Gtk::ICON_SIZE_BUTTON);
    Gtk::RadioButton radiobutton_webs(_("To WebSite"));
    Gtk::Entry entry_webs;
    entry_webs.set_text(link_entries.webs);
    hbox_webs.pack_start(image_webs, false, false);
    hbox_webs.pack_start(radiobutton_webs, false, false);
    hbox_webs.pack_start(entry_webs);
    hbox_webs.set_spacing(5);

    Gtk::HBox hbox_file;
    Gtk::Image image_file;
    image_file.set_from_icon_name(Gtk::Stock::FILE.id, Gtk::ICON_SIZE_BUTTON);
    Gtk::RadioButton radiobutton_file(_("To File"));
    radiobutton_file.join_group(radiobutton_webs);
    Gtk::Entry entry_file;
    entry_file.set_text(link_entries.file);
    Gtk::Button button_browse_file;
    button_browse_file.set_image_from_icon_name("find", Gtk::ICON_SIZE_BUTTON);
    hbox_file.pack_start(image_file, false, false);
    hbox_file.pack_start(radiobutton_file, false, false);
    hbox_file.pack_start(entry_file);
    hbox_file.pack_start(button_browse_file, false, false);
    hbox_file.set_spacing(5);

    Gtk::HBox hbox_folder;
    Gtk::Image image_folder;
    image_folder.set_from_icon_name(Gtk::Stock::DIRECTORY.id, Gtk::ICON_SIZE_BUTTON);
    Gtk::RadioButton radiobutton_folder(_("To Folder"));
    radiobutton_folder.join_group(radiobutton_webs);
    Gtk::Entry entry_folder;
    entry_folder.set_text(link_entries.fold);
    Gtk::Button button_browse_folder;
    button_browse_folder.set_image_from_icon_name("find", Gtk::ICON_SIZE_BUTTON);
    hbox_folder.pack_start(image_folder, false, false);
    hbox_folder.pack_start(radiobutton_folder, false, false);
    hbox_folder.pack_start(entry_folder);
    hbox_folder.pack_start(button_browse_folder, false, false);
    hbox_folder.set_spacing(5);

    Gtk::HBox hbox_node;
    Gtk::Image image_node;
    image_node.set_from_icon_name("cherrytree", Gtk::ICON_SIZE_BUTTON);
    Gtk::RadioButton radiobutton_node(_("To Node"));
    radiobutton_node.join_group(radiobutton_webs);
    hbox_node.pack_start(image_node, false, false);
    hbox_node.pack_start(radiobutton_node);
    hbox_node.set_spacing(5);

    Gtk::HBox hbox_detail;

    Gtk::TreeView treeview_2(ctMainWin.get_tree_store().get_store());
    treeview_2.set_headers_visible(false);
    treeview_2.set_search_column(1);
    Gtk::CellRendererPixbuf renderer_pixbuf_2;
    Gtk::CellRendererText renderer_text_2;
    Gtk::TreeViewColumn column_2;
    treeview_2.append_column("", ctMainWin.get_tree_store().get_columns().rColPixbuf);
    treeview_2.append_column("", ctMainWin.get_tree_store().get_columns().colNodeName);
    Gtk::ScrolledWindow scrolledwindow;
    scrolledwindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    scrolledwindow.add(treeview_2);

    Gtk::VBox vbox_anchor;
    Gtk::Label label_over;
    Gtk::Label label_below;

    Gtk::HBox hbox_anchor;
    Gtk::Entry entry_anchor;
    entry_anchor.set_text(link_entries.anch);
    Gtk::Button button_browse_anchor;
    button_browse_anchor.set_image_from_icon_name("anchor", Gtk::ICON_SIZE_BUTTON);
    hbox_anchor.pack_start(entry_anchor);
    hbox_anchor.pack_start(button_browse_anchor, false, false);

    Gtk::Frame frame_anchor(std::string("<b>")+_("Anchor Name (optional)")+"</b>");
    dynamic_cast<Gtk::Label*>(frame_anchor.get_label_widget())->set_use_markup(true);
    frame_anchor.set_shadow_type(Gtk::SHADOW_NONE);
    frame_anchor.add(hbox_anchor);

    vbox_anchor.pack_start(label_over);
    vbox_anchor.pack_start(frame_anchor, false, false);
    vbox_anchor.pack_start(label_below);

    hbox_detail.pack_start(scrolledwindow);
    hbox_detail.pack_start(vbox_anchor, false, false);

    Gtk::Box* pContentArea = dialog.get_content_area();
    pContentArea->pack_start(hbox_webs, false, false);
    pContentArea->pack_start(hbox_file, false, false);
    pContentArea->pack_start(hbox_folder, false, false);
    pContentArea->pack_start(hbox_node, false, false);
    pContentArea->pack_start(hbox_detail);
    pContentArea->set_spacing(5);

    radiobutton_webs.set_active(link_entries.type == CtConst::LINK_TYPE_WEBS);
    radiobutton_node.set_active(link_entries.type == CtConst::LINK_TYPE_NODE);
    radiobutton_file.set_active(link_entries.type == CtConst::LINK_TYPE_FILE);
    radiobutton_folder.set_active(link_entries.type == CtConst::LINK_TYPE_FOLD);

    bool first_in{true};

    auto link_type_changed_on_dialog = [&]()
    {
        entry_webs.set_sensitive(link_entries.type == CtConst::LINK_TYPE_WEBS);
        hbox_detail.set_sensitive(link_entries.type == CtConst::LINK_TYPE_NODE);
        entry_file.set_sensitive(link_entries.type == CtConst::LINK_TYPE_FILE);
        entry_folder.set_sensitive(link_entries.type == CtConst::LINK_TYPE_FOLD);
        if (link_entries.type == CtConst::LINK_TYPE_WEBS)
        {
            entry_webs.grab_focus();
        }
        else if (link_entries.type == CtConst::LINK_TYPE_NODE)
        {
            treeview_2.grab_focus();
            if (first_in)
            {
                first_in = false;
                std::string exp_colpsd_str = ctTreestore.get_tree_expanded_collapsed_string(ctMainWin.get_tree_view());
                ctTreestore.set_tree_expanded_collapsed_string(exp_colpsd_str, treeview_2, CtApp::P_ctCfg->nodesBookmExp);
            }
            if (!sel_tree_iter)
            {
                sel_tree_iter = ctTreestore.get_iter_first();
            }
            Gtk::TreePath sel_path = ctTreestore.get_path(sel_tree_iter);
            treeview_2.expand_to_path(sel_path);
            treeview_2.set_cursor(sel_path);
            treeview_2.scroll_to_row(sel_path);
        }
        else if (link_entries.type == CtConst::LINK_TYPE_FILE)
        {
            entry_file.grab_focus();
        }
        else
        {
            entry_folder.grab_focus();
        }
    };

    radiobutton_webs.signal_toggled().connect([&]()
    {
        if (radiobutton_webs.get_active())
        {
            link_entries.type = CtConst::LINK_TYPE_WEBS;
        }
        link_type_changed_on_dialog();
    });
    radiobutton_node.signal_toggled().connect([&]()
    {
        if (radiobutton_node.get_active())
        {
            link_entries.type = CtConst::LINK_TYPE_NODE;
        }
        link_type_changed_on_dialog();
    });
    radiobutton_file.signal_toggled().connect([&]()
    {
        if (radiobutton_file.get_active())
        {
            link_entries.type = CtConst::LINK_TYPE_FILE;
        }
        link_type_changed_on_dialog();
    });
    radiobutton_folder.signal_toggled().connect([&]()
    {
        if (radiobutton_folder.get_active())
        {
            link_entries.type = CtConst::LINK_TYPE_FOLD;
        }
        link_type_changed_on_dialog();
    });
    button_browse_file.signal_clicked().connect([&]()
    {
        std::string filepath = file_select_dialog({.pParent=&dialog, .curr_folder=CtApp::P_ctCfg->pickDirFile});
        if (filepath.empty())
        {
            return;
        }
        CtApp::P_ctCfg->pickDirFile = Glib::path_get_dirname(filepath);
        if (CtApp::P_ctCfg->linksRelative)
        {
            Glib::RefPtr<Gio::File> rFile = Gio::File::create_for_path(filepath);
            Glib::RefPtr<Gio::File> rDir = Gio::File::create_for_path(ctMainWin.get_curr_doc_file_dir());
            filepath = rFile->get_relative_path(rDir);
        }
        entry_file.set_text(filepath);
    });
    button_browse_folder.signal_clicked().connect([&]()
    {
        std::string filepath = CtDialogs::folder_select_dialog(CtApp::P_ctCfg->pickDirFile, &dialog);
        if (filepath.empty())
        {
            return;
        }
        CtApp::P_ctCfg->pickDirFile = filepath;
        if (CtApp::P_ctCfg->linksRelative)
        {
            Glib::RefPtr<Gio::File> rFile = Gio::File::create_for_path(filepath);
            Glib::RefPtr<Gio::File> rDir = Gio::File::create_for_path(ctMainWin.get_curr_doc_file_dir());
            filepath = rFile->get_relative_path(rDir);
        }
        entry_folder.set_text(filepath);
    });
    button_browse_anchor.signal_clicked().connect([&]()
    {
        if (!sel_tree_iter)
        {
            CtDialogs::warning_dialog(_("No Node is Selected"), dialog);
            return;
        }
        CtTreeIter ctTreeIter = ctTreestore.to_ct_tree_iter(sel_tree_iter);
        std::list<Glib::ustring> anchors_list;
        for (CtAnchoredWidget* pAnchoredWidget : ctTreeIter.get_all_embedded_widgets())
        {
            if (CtAnchWidgType::ImageAnchor == pAnchoredWidget->get_type())
            {
                anchors_list.push_back(dynamic_cast<CtImageAnchor*>(pAnchoredWidget)->get_anchor_name());
            }
        }
        if (anchors_list.empty())
        {
            info_dialog(_("There are No Anchors in the Selected Node"), dialog);
        }
        else
        {
            Glib::RefPtr<CtChooseDialogListStore> rItemStore = CtChooseDialogListStore::create();
            for (const Glib::ustring& anchName : anchors_list)
            {
                rItemStore->add_row("", "", anchName);
            }
            Gtk::TreeIter res = CtDialogs::choose_item_dialog(dialog, _("Choose Existing Anchor"), rItemStore, _("Anchor Name"));
            if (res)
            {
                Glib::ustring anchName = res->get_value(rItemStore->columns.desc);
                entry_anchor.set_text(anchName);
            }
        }
    });
    treeview_2.signal_event_after().connect([&](GdkEvent* event)
    {
        if ( (event->type != GDK_BUTTON_PRESS) &&
             (event->type != GDK_2BUTTON_PRESS) &&
             (event->type != GDK_KEY_PRESS) )
        {
            return;
        }
        sel_tree_iter = treeview_2.get_selection()->get_selected();
        if ( (event->type == GDK_BUTTON_PRESS) &&
             (event->button.button == 2) )
        {
            Gtk::TreePath path_at_click;
            if (treeview_2.get_path_at_pos((int)event->button.x, (int)event->button.y, path_at_click))
            {
                if (treeview_2.row_expanded(path_at_click))
                    treeview_2.collapse_row(path_at_click);
                else
                    treeview_2.expand_row(path_at_click, true);
            }
        }
        else if ( (event->type == GDK_2BUTTON_PRESS) &&
                  (event->button.button == 1) &&
                  sel_tree_iter )
        {
            Gtk::TreePath path = ctTreestore.get_path(sel_tree_iter);
            if (treeview_2.row_expanded(path))
                treeview_2.collapse_row(path);
            else
                treeview_2.expand_row(path, true);
        }
        else if (event->type == GDK_KEY_PRESS && sel_tree_iter)
        {
            Gtk::TreePath path = ctTreestore.get_path(sel_tree_iter);
            if (event->key.keyval == GDK_KEY_Left)
                treeview_2.collapse_row(path);
            else if (event->key.keyval == GDK_KEY_Right)
                treeview_2.expand_row(path, false);
        }
    });
    dialog.signal_key_press_event().connect([&](GdkEventKey* event)
    {
        if (event->keyval == GDK_KEY_Tab)
        {
            if (link_entries.type == CtConst::LINK_TYPE_WEBS) radiobutton_file.set_active(true);
            else if (link_entries.type == CtConst::LINK_TYPE_FILE) radiobutton_folder.set_active(true);
            else if (link_entries.type == CtConst::LINK_TYPE_FOLD) radiobutton_node.set_active(true);
            else radiobutton_webs.set_active(true);
            return true;
        }
        return false;
    });

    pContentArea->show_all();
    link_type_changed_on_dialog();

    if (dialog.run() != GTK_RESPONSE_ACCEPT)
    {
        return false;
    }

    link_entries.webs = str::trim(entry_webs.get_text());
    link_entries.file = str::trim(entry_file.get_text());
    link_entries.fold = str::trim(entry_folder.get_text());
    link_entries.anch = str::trim(entry_anchor.get_text());
    link_entries.node_id = ctTreestore.to_ct_tree_iter(sel_tree_iter).get_node_id();
    return true;
}

// The Select file dialog, Returns the retrieved filepath or None
std::string CtDialogs::file_select_dialog(const file_select_args& args)
{
    Gtk::FileChooserDialog chooser(_("Select File"),
                                   Gtk::FILE_CHOOSER_ACTION_OPEN);
    chooser.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    chooser.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_ACCEPT);
    if (args.pParent)
    {
        chooser.set_transient_for(*args.pParent);
        chooser.property_modal() = true;
        chooser.property_destroy_with_parent() = true;
        chooser.set_position(Gtk::WIN_POS_CENTER_ON_PARENT);
    }
    else
    {
        chooser.set_position(Gtk::WIN_POS_CENTER);
    }
    if (args.curr_folder.empty() || !Glib::file_test(args.curr_folder, Glib::FILE_TEST_IS_DIR))
    {
        chooser.set_current_folder(g_get_home_dir());
    }
    else
    {
        chooser.set_current_folder(args.curr_folder);
    }
    if (args.filter_pattern.size() || args.filter_mime.size())
    {
        Glib::RefPtr<Gtk::FileFilter> rFileFilter = Gtk::FileFilter::create();
        rFileFilter->set_name(args.filter_name);
        for (const std::string& element : args.filter_pattern)
        {
            rFileFilter->add_pattern(element);
        }
        for (const std::string& element : args.filter_mime)
        {
            rFileFilter->add_mime_type(element);
        }
        chooser.add_filter(rFileFilter);
    }
    return (chooser.run() == Gtk::RESPONSE_ACCEPT ? chooser.get_filename() : "");
}

// The Select folder dialog, returns the retrieved folderpath or None
std::string CtDialogs::folder_select_dialog(const std::string& curr_folder,
                                            Gtk::Window* pParent /*= nullptr*/)
{
    Gtk::FileChooserDialog chooser(_("Select Folder"),
                                   Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
    chooser.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    chooser.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_ACCEPT);
    if (pParent)
    {
        chooser.set_transient_for(*pParent);
        chooser.property_modal() = true;
        chooser.property_destroy_with_parent() = true;
        chooser.set_position(Gtk::WIN_POS_CENTER_ON_PARENT);
    }
    else
    {
        chooser.set_position(Gtk::WIN_POS_CENTER);
    }
    if (curr_folder.empty() || !Glib::file_test(curr_folder, Glib::FILE_TEST_IS_DIR))
    {
        chooser.set_current_folder(g_get_home_dir());
    }
    else
    {
        chooser.set_current_folder(curr_folder);
    }
    return (chooser.run() == Gtk::RESPONSE_ACCEPT ? chooser.get_filename() : "");
}

// The Save file as dialog, Returns the retrieved filepath or None
std::string CtDialogs::file_save_as_dialog(const file_select_args& args)
{
    Gtk::FileChooserDialog chooser(_("Save File as"),
                                   Gtk::FILE_CHOOSER_ACTION_SAVE);
    chooser.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    chooser.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);
    chooser.set_do_overwrite_confirmation(true);
    if (args.pParent)
    {
        chooser.set_transient_for(*args.pParent);
        chooser.property_modal() = true;
        chooser.property_destroy_with_parent() = true;
        chooser.set_position(Gtk::WIN_POS_CENTER_ON_PARENT);
    }
    else
    {
        chooser.set_position(Gtk::WIN_POS_CENTER);
    }
    if (args.curr_folder.empty() || !Glib::file_test(args.curr_folder, Glib::FILE_TEST_IS_DIR))
    {
        chooser.set_current_folder(g_get_home_dir());
    }
    else
    {
        chooser.set_current_folder(args.curr_folder);
    }
    if (!args.curr_file_name.empty())
    {
        chooser.set_current_name(args.curr_file_name);
    }
    if (!args.filter_pattern.empty())
    {
        Glib::RefPtr<Gtk::FileFilter> rFileFilter = Gtk::FileFilter::create();
        rFileFilter->set_name(args.filter_name);
        for (const std::string& element : args.filter_pattern)
        {
            rFileFilter->add_pattern(element);
        }
        for (const std::string& element : args.filter_mime)
        {
            rFileFilter->add_mime_type(element);
        }
        chooser.add_filter(rFileFilter);
    }
    return (chooser.run() == Gtk::RESPONSE_ACCEPT ? chooser.get_filename() : "");
}

// Insert/Edit Image
Glib::RefPtr<Gdk::Pixbuf> CtDialogs::image_handle_dialog(Gtk::Window& parent_win,
                                                         const Glib::ustring& title,
                                                         Glib::RefPtr<Gdk::Pixbuf> rOriginalPixbuf)
{
    int width = rOriginalPixbuf->get_width();
    int height = rOriginalPixbuf->get_height();
    double image_w_h_ration = double(width)/height;

    Gtk::Dialog dialog(title,
                       parent_win,
                       Gtk::DialogFlags::DIALOG_MODAL | Gtk::DialogFlags::DIALOG_DESTROY_WITH_PARENT);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_REJECT);
    Gtk::Button* pOK_button = dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);
    dialog.set_default_response(Gtk::RESPONSE_ACCEPT);
    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
    dialog.set_default_size(600, 500);
    Gtk::Button button_rotate_90_ccw;
    button_rotate_90_ccw.set_image_from_icon_name("object-rotate-left", Gtk::ICON_SIZE_DND);
    Gtk::Button button_rotate_90_cw;
    button_rotate_90_cw.set_image_from_icon_name("object-rotate-right", Gtk::ICON_SIZE_DND);
    Gtk::ScrolledWindow scrolledwindow;
    scrolledwindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    Glib::RefPtr<Gtk::Adjustment> rHAdj = Gtk::Adjustment::create(width, 1, height, 1);
    Glib::RefPtr<Gtk::Adjustment> rVAdj = Gtk::Adjustment::create(width, 1, width, 1);
    Gtk::Viewport viewport(rHAdj, rVAdj);
    Gtk::Image image(rOriginalPixbuf);
    scrolledwindow.add(viewport);
    viewport.add(image);
    Gtk::HBox hbox_1;
    hbox_1.pack_start(button_rotate_90_ccw, false, false);
    hbox_1.pack_start(scrolledwindow);
    hbox_1.pack_start(button_rotate_90_cw, false, false);
    hbox_1.set_spacing(2);
    Gtk::Label label_width(_("Width"));
    Glib::RefPtr<Gtk::Adjustment> rAdj_width = Gtk::Adjustment::create(width, 1, 10000, 1);
    Gtk::SpinButton spinbutton_width(rAdj_width);
    Gtk::Label label_height(_("Height"));
    Glib::RefPtr<Gtk::Adjustment> rAdj_height = Gtk::Adjustment::create(height, 1, 10000, 1);
    Gtk::SpinButton spinbutton_height(rAdj_height);
    Gtk::HBox hbox_2;
    hbox_2.pack_start(label_width);
    hbox_2.pack_start(spinbutton_width);
    hbox_2.pack_start(label_height);
    hbox_2.pack_start(spinbutton_height);
    Gtk::Box* pContentArea = dialog.get_content_area();
    pContentArea->pack_start(hbox_1);
    pContentArea->pack_start(hbox_2, false, false);
    pContentArea->set_spacing(6);

    auto image_load_into_dialog = [&]()
    {
        spinbutton_width.set_value(width);
        spinbutton_height.set_value(height);
        Glib::RefPtr<Gdk::Pixbuf> rPixbuf;
        if (width <= 900 && height <= 600)
        {
            // original size into the dialog
            rPixbuf = rOriginalPixbuf->scale_simple(width, height, Gdk::INTERP_BILINEAR);
        }
        else
        {
            // reduced size visible into the dialog
            if (width > 900)
            {
                int img_parms_width = 900;
                int img_parms_height = (int)(img_parms_width / image_w_h_ration);
                rPixbuf = rOriginalPixbuf->scale_simple(img_parms_width, img_parms_height, Gdk::INTERP_BILINEAR);
            }
            else
            {
                int img_parms_height = 600;
                int img_parms_width = (int)(img_parms_height * image_w_h_ration);
                rPixbuf = rOriginalPixbuf->scale_simple(img_parms_width, img_parms_height, Gdk::INTERP_BILINEAR);
            }
        }
        image.set(rPixbuf);
    };
    button_rotate_90_cw.signal_clicked().connect([&]()
    {
        rOriginalPixbuf = rOriginalPixbuf->rotate_simple(Gdk::PixbufRotation::PIXBUF_ROTATE_CLOCKWISE);
        image_w_h_ration = 1./image_w_h_ration;
        std::swap(width, height); // new width is the former height and vice versa
        image_load_into_dialog();
    });
    button_rotate_90_ccw.signal_clicked().connect([&]()
    {
        rOriginalPixbuf = rOriginalPixbuf->rotate_simple(Gdk::PixbufRotation::PIXBUF_ROTATE_COUNTERCLOCKWISE);
        image_w_h_ration = 1./image_w_h_ration;
        std::swap(width, height); // new width is the former height and vice versa
        image_load_into_dialog();
    });
    spinbutton_width.signal_value_changed().connect([&]()
    {
        width = spinbutton_width.get_value_as_int();
        height = (int)(width/image_w_h_ration);
        image_load_into_dialog();
    });
    spinbutton_height.signal_value_changed().connect([&]()
    {
        height = spinbutton_height.get_value_as_int();
        width = (int)(height*image_w_h_ration);
        image_load_into_dialog();
    });
    image_load_into_dialog();
    pContentArea->show_all();
    pOK_button->grab_focus();

    return (Gtk::RESPONSE_ACCEPT == dialog.run() ? rOriginalPixbuf->scale_simple(width, height, Gdk::INTERP_BILINEAR) : Glib::RefPtr<Gdk::Pixbuf>());
}

// Opens the CodeBox Handle Dialog
bool CtDialogs::codeboxhandle_dialog(Gtk::Window& parent_win,
                                     const Glib::ustring& title)
{
    Gtk::Dialog dialog(title,
                       parent_win,
                       Gtk::DialogFlags::DIALOG_MODAL | Gtk::DialogFlags::DIALOG_DESTROY_WITH_PARENT);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_REJECT);
    dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);
    dialog.set_default_size(300, -1);
    dialog.set_position(Gtk::WIN_POS_CENTER_ON_PARENT);

    CtConfig* pConfig = CtApp::P_ctCfg;

    Gtk::Button button_prog_lang;
    Glib::ustring button_label = (pConfig->codeboxSynHighl != CtConst::PLAIN_TEXT_ID ? pConfig->codeboxSynHighl : pConfig->autoSynHighl);
    std::string button_stock_id = CtConst::getStockIdForCodeType(button_label);
    button_prog_lang.set_label(button_label);
    button_prog_lang.set_image(*CtImage::new_image_from_stock(button_stock_id, Gtk::ICON_SIZE_MENU));
    Gtk::RadioButton radiobutton_plain_text(_("Plain Text"));
    Gtk::RadioButton radiobutton_auto_syntax_highl(_("Automatic Syntax Highlighting"));
    radiobutton_auto_syntax_highl.join_group(radiobutton_plain_text);
    if (pConfig->codeboxSynHighl == CtConst::PLAIN_TEXT_ID)
    {
        radiobutton_plain_text.set_active(true);
        button_prog_lang.set_sensitive(false);
    }
    else
    {
        radiobutton_auto_syntax_highl.set_active(true);
    }
    Gtk::VBox type_vbox;
    type_vbox.pack_start(radiobutton_plain_text);
    type_vbox.pack_start(radiobutton_auto_syntax_highl);
    type_vbox.pack_start(button_prog_lang);
    Gtk::Frame type_frame(std::string("<b>")+_("Type")+"</b>");
    dynamic_cast<Gtk::Label*>(type_frame.get_label_widget())->set_use_markup(true);
    type_frame.set_shadow_type(Gtk::SHADOW_NONE);
    type_frame.add(type_vbox);

    Gtk::Label label_width(_("Width"));
    Glib::RefPtr<Gtk::Adjustment> rAdj_width = Gtk::Adjustment::create(pConfig->codeboxWidth, 1, 10000);
    Gtk::SpinButton spinbutton_width(rAdj_width);
    spinbutton_width.set_value(pConfig->codeboxWidth);
    Gtk::Label label_height(_("Height"));
    Glib::RefPtr<Gtk::Adjustment> rAdj_height = Gtk::Adjustment::create(pConfig->codeboxHeight, 1, 10000);
    Gtk::SpinButton spinbutton_height(rAdj_height);
    spinbutton_height.set_value(pConfig->codeboxHeight);

    Gtk::RadioButton radiobutton_codebox_pixels(_("pixels"));
    Gtk::RadioButton radiobutton_codebox_percent("%");
    radiobutton_codebox_percent.join_group(radiobutton_codebox_pixels);
    radiobutton_codebox_pixels.set_active(pConfig->codeboxWidthPixels);
    radiobutton_codebox_percent.set_active(!pConfig->codeboxWidthPixels);

    Gtk::VBox vbox_pix_perc;
    vbox_pix_perc.pack_start(radiobutton_codebox_pixels);
    vbox_pix_perc.pack_start(radiobutton_codebox_percent);
    Gtk::HBox hbox_width;
    hbox_width.pack_start(label_width, false, false);
    hbox_width.pack_start(spinbutton_width, false, false);
    hbox_width.pack_start(vbox_pix_perc);
    hbox_width.set_spacing(5);
    Gtk::HBox hbox_height;
    hbox_height.pack_start(label_height, false, false);
    hbox_height.pack_start(spinbutton_height, false, false);
    hbox_height.set_spacing(5);
    Gtk::VBox vbox_size;
    vbox_size.pack_start(hbox_width);
    vbox_size.pack_start(hbox_height);
    Gtk::Alignment size_align;
    size_align.set_padding(0, 6, 6, 6);
    size_align.add(vbox_size);

    Gtk::Frame size_frame(std::string("<b>")+_("Size")+"</b>");
    dynamic_cast<Gtk::Label*>(size_frame.get_label_widget())->set_use_markup(true);
    size_frame.set_shadow_type(Gtk::SHADOW_NONE);
    size_frame.add(size_align);

    Gtk::CheckButton checkbutton_codebox_linenumbers(_("Show Line Numbers"));
    checkbutton_codebox_linenumbers.set_active(pConfig->codeboxLineNum);
    Gtk::CheckButton checkbutton_codebox_matchbrackets(_("Highlight Matching Brackets"));
    checkbutton_codebox_matchbrackets.set_active(pConfig->codeboxMatchBra);
    Gtk::VBox vbox_options;
    vbox_options.pack_start(checkbutton_codebox_linenumbers);
    vbox_options.pack_start(checkbutton_codebox_matchbrackets);
    Gtk::Alignment opt_align;
    opt_align.set_padding(6, 6, 6, 6);
    opt_align.add(vbox_options);

    Gtk::Frame options_frame(std::string("<b>")+_("Options")+"</b>");
    dynamic_cast<Gtk::Label*>(options_frame.get_label_widget())->set_use_markup(true);
    options_frame.set_shadow_type(Gtk::SHADOW_NONE);
    options_frame.add(opt_align);

    Gtk::Box* pContentArea = dialog.get_content_area();
    pContentArea->set_spacing(5);
    pContentArea->pack_start(type_frame);
    pContentArea->pack_start(size_frame);
    pContentArea->pack_start(options_frame);
    pContentArea->show_all();

    button_prog_lang.signal_clicked().connect([&button_prog_lang, &dialog]()
    {
        Glib::RefPtr<CtChooseDialogListStore> rItemStore = CtChooseDialogListStore::create();
        for (const std::string& lang : CtApp::R_languageManager->get_language_ids())
        {
            rItemStore->add_row(CtConst::getStockIdForCodeType(lang), "", lang);
        }
        Gtk::TreeIter res = CtDialogs::choose_item_dialog(dialog, _("Automatic Syntax Highlighting"), rItemStore);
        if (res)
        {
            std::string stock_id = res->get_value(rItemStore->columns.desc);
            button_prog_lang.set_label(stock_id);
            button_prog_lang.set_image(*CtImage::new_image_from_stock(stock_id, Gtk::ICON_SIZE_MENU));
        }
    });
    radiobutton_auto_syntax_highl.signal_toggled().connect([&radiobutton_auto_syntax_highl, &button_prog_lang]()
    {
        button_prog_lang.set_sensitive(radiobutton_auto_syntax_highl.get_active());
    });
    dialog.signal_key_press_event().connect([&](GdkEventKey* key)
    {
        if (key->keyval == GDK_KEY_Return)
        {
            spinbutton_width.update();
            spinbutton_height.update();
            dialog.response(Gtk::RESPONSE_ACCEPT);
            return true;
        }
        return false;
    });
    radiobutton_codebox_pixels.signal_toggled().connect([&radiobutton_codebox_pixels, &spinbutton_width]()
    {
        if (radiobutton_codebox_pixels.get_active())
        {
            spinbutton_width.set_value(700);
        }
        else if (spinbutton_width.get_value() > 100)
        {
            spinbutton_width.set_value(90);
        }
    });

    const int response = dialog.run();
    dialog.hide();

    if (response == Gtk::RESPONSE_ACCEPT)
    {
        pConfig->codeboxWidth = spinbutton_width.get_value_as_int();
        pConfig->codeboxWidthPixels = radiobutton_codebox_pixels.get_active();
        pConfig->codeboxHeight = spinbutton_height.get_value();
        pConfig->codeboxLineNum = checkbutton_codebox_linenumbers.get_active();
        pConfig->codeboxMatchBra = checkbutton_codebox_matchbrackets.get_active();
        if (radiobutton_plain_text.get_active())
        {
            pConfig->codeboxSynHighl = CtConst::PLAIN_TEXT_ID;
        }
        else
        {
            pConfig->codeboxSynHighl = button_prog_lang.get_label();
        }
        return true;
    }
    return false;
}
