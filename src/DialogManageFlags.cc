/* Copyright (C) 2004 Bart
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#include "../include/DialogManageFlags.h"

#include <gtkmm/main.h>
#include <gtkmm/stock.h>

namespace GParted
{

DialogManageFlags::DialogManageFlags( const Partition & partition )
{
	any_change = false ;

	set_title( String::ucompose( _("Manage flags on %1"), partition .get_path() ) );
	set_has_separator( false ) ;
	set_size_request( 300, -1 ) ;
	set_resizable( false );
	
	Glib::ustring str_temp = "<span weight=\"bold\" size=\"larger\">" ;
	str_temp += String::ucompose( _("Manage flags on %1"), partition .get_path() ) ;
	str_temp += "</span>\n" ;
	get_vbox() ->pack_start( * Utils::mk_label( str_temp ), Gtk::PACK_SHRINK );
	
	//setup treeview
	liststore_flags = Gtk::ListStore::create( treeview_flags_columns ) ;
	treeview_flags .set_model( liststore_flags ) ;
	treeview_flags .set_headers_visible( false ) ;

	treeview_flags .append_column( "", treeview_flags_columns .status ) ;
	treeview_flags .append_column( "", treeview_flags_columns .flag ) ;
	static_cast<Gtk::CellRendererToggle *>( treeview_flags .get_column_cell_renderer( 0 ) ) 
		->property_activatable() = true ;
	static_cast<Gtk::CellRendererToggle *>( treeview_flags .get_column_cell_renderer( 0 ) ) 
		->signal_toggled() .connect( sigc::mem_fun( *this, &DialogManageFlags::on_flag_toggled ) ) ;

	get_vbox() ->pack_start( treeview_flags, Gtk::PACK_SHRINK ) ;

	this ->partition = partition ;
	
	add_button( Gtk::Stock::CLOSE, Gtk::RESPONSE_OK ) ->grab_focus() ;
		
	show_all_children() ;
}

void DialogManageFlags::on_show()
{
	Dialog::on_show() ;

	while ( Gtk::Main::events_pending() )
		Gtk::Main::iteration() ;

	load_flags() ;
}

void DialogManageFlags::load_flags()
{
	liststore_flags ->clear() ;

	std::map<Glib::ustring, bool> flag_info = signal_get_flags .emit( partition ) ;
	
	for ( std::map<Glib::ustring, bool>::iterator iter = flag_info .begin() ; iter != flag_info .end() ; ++iter )
	{
		row = *( liststore_flags ->append() ) ;
		row[ treeview_flags_columns .flag ] = iter ->first ;
		row[ treeview_flags_columns .status ] = iter ->second ;
	}
}

void DialogManageFlags::on_flag_toggled( const Glib::ustring & path ) 
{
	any_change = true ;

	row = *( liststore_flags ->get_iter( path ) ) ;
	row[ treeview_flags_columns .status ] = ! row[ treeview_flags_columns .status ] ;

	signal_toggle_flag .emit( partition, row[ treeview_flags_columns .flag ], row[ treeview_flags_columns .status ] ) ;

	load_flags() ;
}

}//GParted