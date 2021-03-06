/* Copyright (C) 2004 Bart
 * Copyright (C) 2008, 2009, 2010 Curtis Gedak
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
 
 
#include "../include/reiserfs.h"

namespace GParted
{

FS reiserfs::get_filesystem_support()
{
	FS fs ;
	fs .filesystem = GParted::FS_REISERFS ;

	fs .busy = FS::GPARTED ;

	if ( ! Glib::find_program_in_path( "debugreiserfs" ) .empty() )
	{
		fs .read = GParted::FS::EXTERNAL ;
		fs .read_label = FS::EXTERNAL ;
	}

	if ( ! Glib::find_program_in_path( "reiserfstune" ) .empty() )
	{
		fs .write_label = FS::EXTERNAL ;
		fs .read_uuid = FS::EXTERNAL ;
		fs .write_uuid = FS::EXTERNAL ;
	}

	if ( ! Glib::find_program_in_path( "mkreiserfs" ) .empty() )
	{
		fs .create = GParted::FS::EXTERNAL ;
		fs .create_with_label = GParted::FS::EXTERNAL ;
	}
	
	if ( ! Glib::find_program_in_path( "reiserfsck" ) .empty() )
		fs .check = GParted::FS::EXTERNAL ;
		
	//resizing is a delicate process ...
	if ( ! Glib::find_program_in_path( "resize_reiserfs" ) .empty() && fs .check )
	{
		fs .grow = GParted::FS::EXTERNAL ;
		
		if ( fs .read ) //needed to determine a min file system size..
			fs .shrink = GParted::FS::EXTERNAL ;
	}

	if ( fs .check )
	{
		fs .copy = GParted::FS::GPARTED ;
		fs .move = GParted::FS::GPARTED ;
	}

	fs .online_read = FS::GPARTED ;
#ifdef ENABLE_ONLINE_RESIZE
	if ( Utils::kernel_version_at_least( 3, 6, 0 ) )
		fs. online_grow = fs. grow ;
#endif

	//Actual minimum is at least 18 blocks larger than 32 MiB for the journal offset
	fs .MIN = 34 * MEBIBYTE ;
	
	return fs ;
}

void reiserfs::set_used_sectors( Partition & partition ) 
{
	if ( ! Utils::execute_command( "debugreiserfs " + partition .get_path(), output, error, true ) )
	{
		index = output .find( "Count of blocks on the device:" ) ;
		if ( index >= output .length() ||
		     sscanf( output .substr( index ) .c_str(), "Count of blocks on the device: %Ld", &T ) != 1 )
			T = -1 ;

		index = output .find( "Blocksize:" ) ;
		if ( index >= output .length() || 
		     sscanf( output .substr( index ) .c_str(), "Blocksize: %Ld", &S ) != 1 )
			S = -1 ;

		index = output .find( ":", output .find( "Free blocks" ) ) +1 ;
		if ( index >= output .length() ||
		     sscanf( output .substr( index ) .c_str(), "%Ld", &N ) != 1 )
			N = -1 ;

		if ( T > -1 && N > -1 && S > -1 )
		{
			T = Utils::round( T * ( S / double(partition .sector_size) ) ) ;
			N = Utils::round( N * ( S / double(partition .sector_size) ) ) ;
			partition .set_sector_usage( T, N ) ;
		}
	}
	else
	{
		if ( ! output .empty() )
			partition .messages .push_back( output ) ;
		
		if ( ! error .empty() )
			partition .messages .push_back( error ) ;
	}
}

void reiserfs::read_label( Partition & partition )
{
	if ( ! Utils::execute_command( "debugreiserfs " + partition .get_path(), output, error, true ) )
	{
		partition.set_filesystem_label( Utils::regexp_label( output, "^label:[\t ]*(.*)$" ) );
	}
	else
	{
		if ( ! output .empty() )
			partition .messages .push_back( output ) ;
		
		if ( ! error .empty() )
			partition .messages .push_back( error ) ;
	}
}
	
bool reiserfs::write_label( const Partition & partition, OperationDetail & operationdetail )
{
	return ! execute_command( "reiserfstune --label \"" + partition.get_filesystem_label() + "\" " +
	                          partition.get_path(),
	                          operationdetail );
}

void reiserfs::read_uuid( Partition & partition )
{
	if ( ! Utils::execute_command( "debugreiserfs " + partition .get_path(), output, error, true ) )
	{
		partition .uuid = Utils::regexp_label( output, "^UUID:[[:blank:]]*(" RFC4122_NONE_NIL_UUID_REGEXP ")" ) ;
	}
	else
	{
		if ( ! output .empty() )
			partition .messages .push_back( output ) ;

		if ( ! error .empty() )
			partition .messages .push_back( error ) ;
	}
}

bool reiserfs::write_uuid( const Partition & partition, OperationDetail & operationdetail )
{
	return ! execute_command( "reiserfstune -u random " + partition .get_path(), operationdetail ) ;
}

bool reiserfs::create( const Partition & new_partition, OperationDetail & operationdetail )
{
	return ! execute_command( "mkreiserfs -f -f --label \"" + new_partition.get_filesystem_label() + "\" " +
	                          new_partition.get_path(),
	                          operationdetail, false, true );
}

bool reiserfs::resize( const Partition & partition_new, OperationDetail & operationdetail, bool fill_partition )
{ 
	Glib::ustring size = "" ;
	if ( ! fill_partition )
	{
		size = " -s " + Utils::num_to_str( floor( Utils::sector_to_unit(
		                   partition_new .get_sector_length(), partition_new .sector_size, UNIT_BYTE ) ) ) ;
	}
	Glib::ustring cmd = "sh -c 'echo y | resize_reiserfs" + size + " " + partition_new .get_path() + "'" ;

	exit_status = execute_command( cmd, operationdetail ) ;

	return ( exit_status == 0 || exit_status == 256 ) ;
}

bool reiserfs::check_repair( const Partition & partition, OperationDetail & operationdetail )
{
	exit_status = execute_command( "reiserfsck --yes --fix-fixable --quiet " + partition.get_path(),
				       operationdetail, false, true );
	
	return ( exit_status == 0 || exit_status == 1 || exit_status == 256 ) ;
}

} //GParted
