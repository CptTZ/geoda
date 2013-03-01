/**
 * GeoDa TM, Copyright (C) 2011-2013 by Luc Anselin - all rights reserved
 *
 * This file is part of GeoDa.
 * 
 * GeoDa is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GeoDa is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 *  Note: This file is the place to hold all constants such as default colors,
 *  as well as common text strings used in various dialogs.  Everything
 *  should be kept in the GeoDaConst namespace
 *
 */

#ifndef __GEODA_CENTER_GEODACONST_H__
#define __GEODA_CENTER_GEODACONST_H__

#include <vector>
#include <wx/gdicmn.h>
#include <wx/string.h>
#include <wx/colour.h>
#include <wx/font.h>
#include <wx/pen.h>

class GeoDaConst {	
public:
	static const int EMPTY = -1;
	
	// This should be called only once in MyApp::OnInit()
	static void init();
	
	// Types for use in Table
	enum FieldType {
		unknown_type,
		double_type, // N or F with decimals > 0 in DBF
		long64_type, // N or F with decimals = 0 in DBF
		string_type, // C in DBF, max 254 characters
		date_type // D in DBF, YYYYMMDD format
	};
	
	static const int max_dbf_numeric_len = 20; // same for long and double
	static const int max_dbf_long_len = 20;
	static const int min_dbf_long_len = 1;
	static const int default_dbf_long_len = 18;
	static const int max_dbf_double_len = 20;
	static const int min_dbf_double_len = 2; // allow for "0." always
	static const int default_dbf_double_len = 18;
	static const int max_dbf_double_decimals = 15;
	static const int min_dbf_double_decimals = 1;
	static const int default_display_decimals = 4;
	static const int default_dbf_double_decimals = 7;
	static const int max_dbf_string_len = 254;
	static const int min_dbf_string_len = 1;
	static const int default_dbf_string_len = 40;
	static const int max_dbf_date_len = 8;
	static const int min_dbf_date_len = 8;
	static const int default_dbf_date_len = 8;
	
	// Shared menu ids
	static const int ID_TIME_SYNC_VAR1 = wxID_HIGHEST + 1000;
	static const int ID_TIME_SYNC_VAR2 = wxID_HIGHEST + 1001;
	static const int ID_TIME_SYNC_VAR3 = wxID_HIGHEST + 1002;
	static const int ID_TIME_SYNC_VAR4 = wxID_HIGHEST + 1004;
	
	static const int ID_FIX_SCALE_OVER_TIME_VAR1 = wxID_HIGHEST + 2000;
	static const int ID_FIX_SCALE_OVER_TIME_VAR2 = wxID_HIGHEST + 2001;
	static const int ID_FIX_SCALE_OVER_TIME_VAR3 = wxID_HIGHEST + 2002;
	static const int ID_FIX_SCALE_OVER_TIME_VAR4 = wxID_HIGHEST + 2004;
	
	static const int ID_PLOTS_PER_VIEW_1 = wxID_HIGHEST + 3000;
	static const int ID_PLOTS_PER_VIEW_2 = wxID_HIGHEST + 3001;
	static const int ID_PLOTS_PER_VIEW_3 = wxID_HIGHEST + 3002;
	static const int ID_PLOTS_PER_VIEW_4 = wxID_HIGHEST + 3003;
	static const int ID_PLOTS_PER_VIEW_5 = wxID_HIGHEST + 3004;
	static const int ID_PLOTS_PER_VIEW_6 = wxID_HIGHEST + 3005;
	static const int ID_PLOTS_PER_VIEW_7 = wxID_HIGHEST + 3006;
	static const int ID_PLOTS_PER_VIEW_8 = wxID_HIGHEST + 3007;
	static const int ID_PLOTS_PER_VIEW_9 = wxID_HIGHEST + 3008;
	static const int ID_PLOTS_PER_VIEW_10 = wxID_HIGHEST + 3009;
	static const int max_plots_per_view_menu_items = 10;
	static const int ID_PLOTS_PER_VIEW_OTHER = wxID_HIGHEST + 3100;
	static const int ID_PLOTS_PER_VIEW_ALL = wxID_HIGHEST + 3200;
	
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A0 = wxID_HIGHEST + 4000;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A1 = wxID_HIGHEST + 4001;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A2 = wxID_HIGHEST + 4002;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A3 = wxID_HIGHEST + 4003;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A4 = wxID_HIGHEST + 4004;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A5 = wxID_HIGHEST + 4005;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A6 = wxID_HIGHEST + 4006;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A7 = wxID_HIGHEST + 4007;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A8 = wxID_HIGHEST + 4008;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A9 = wxID_HIGHEST + 4009;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A10 = wxID_HIGHEST + 4010;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A11 = wxID_HIGHEST + 4011;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A12 = wxID_HIGHEST + 4012;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A13 = wxID_HIGHEST + 4013;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A14 = wxID_HIGHEST + 4014;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A15 = wxID_HIGHEST + 4015;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A16 = wxID_HIGHEST + 4016;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A17 = wxID_HIGHEST + 4017;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A18 = wxID_HIGHEST + 4018;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A19 = wxID_HIGHEST + 4019;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A20 = wxID_HIGHEST + 4020;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A21 = wxID_HIGHEST + 4021;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A22 = wxID_HIGHEST + 4022;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A23 = wxID_HIGHEST + 4023;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A24 = wxID_HIGHEST + 4024;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A25 = wxID_HIGHEST + 4025;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A26 = wxID_HIGHEST + 4026;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A27 = wxID_HIGHEST + 4027;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A28 = wxID_HIGHEST + 4028;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_A29 = wxID_HIGHEST + 4029;
	
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B0 = wxID_HIGHEST + 4100;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B1 = wxID_HIGHEST + 4101;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B2 = wxID_HIGHEST + 4102;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B3 = wxID_HIGHEST + 4103;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B4 = wxID_HIGHEST + 4104;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B5 = wxID_HIGHEST + 4105;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B6 = wxID_HIGHEST + 4106;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B7 = wxID_HIGHEST + 4107;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B8 = wxID_HIGHEST + 4108;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B9 = wxID_HIGHEST + 4109;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B10 = wxID_HIGHEST + 4110;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B11 = wxID_HIGHEST + 4111;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B12 = wxID_HIGHEST + 4112;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B13 = wxID_HIGHEST + 4113;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B14 = wxID_HIGHEST + 4114;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B15 = wxID_HIGHEST + 4115;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B16 = wxID_HIGHEST + 4116;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B17 = wxID_HIGHEST + 4117;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B18 = wxID_HIGHEST + 4118;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B19 = wxID_HIGHEST + 4119;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B20 = wxID_HIGHEST + 4120;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B21 = wxID_HIGHEST + 4121;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B22 = wxID_HIGHEST + 4122;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B23 = wxID_HIGHEST + 4123;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B24 = wxID_HIGHEST + 4124;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B25 = wxID_HIGHEST + 4125;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B26 = wxID_HIGHEST + 4126;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B27 = wxID_HIGHEST + 4127;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B28 = wxID_HIGHEST + 4128;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_B29 = wxID_HIGHEST + 4129;
	
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C0 = wxID_HIGHEST + 4200;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C1 = wxID_HIGHEST + 4201;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C2 = wxID_HIGHEST + 4202;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C3 = wxID_HIGHEST + 4203;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C4 = wxID_HIGHEST + 4204;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C5 = wxID_HIGHEST + 4205;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C6 = wxID_HIGHEST + 4206;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C7 = wxID_HIGHEST + 4207;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C8 = wxID_HIGHEST + 4208;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C9 = wxID_HIGHEST + 4209;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C10 = wxID_HIGHEST + 4210;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C11 = wxID_HIGHEST + 4211;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C12 = wxID_HIGHEST + 4212;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C13 = wxID_HIGHEST + 4213;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C14 = wxID_HIGHEST + 4214;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C15 = wxID_HIGHEST + 4215;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C16 = wxID_HIGHEST + 4216;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C17 = wxID_HIGHEST + 4217;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C18 = wxID_HIGHEST + 4218;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C19 = wxID_HIGHEST + 4219;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C20 = wxID_HIGHEST + 4220;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C21 = wxID_HIGHEST + 4221;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C22 = wxID_HIGHEST + 4222;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C23 = wxID_HIGHEST + 4223;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C24 = wxID_HIGHEST + 4224;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C25 = wxID_HIGHEST + 4225;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C26 = wxID_HIGHEST + 4226;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C27 = wxID_HIGHEST + 4227;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C28 = wxID_HIGHEST + 4228;
	static const int ID_CUSTOM_CAT_CLASSIF_CHOICE_C29 = wxID_HIGHEST + 4229;
	
	// Standard wxFont pointers.
	static wxFont* extra_small_font;
	static wxFont* small_font;
	static wxFont* medium_font;
	static wxFont* large_font;
	
	// MyShape constants
	static const wxPen* default_myshape_pen;
	static const wxBrush* default_myshape_brush;
	
	// MyPoint radius to give a larger target for clicking on
	static const int my_point_click_radius = 2;
	
	// Shared Colours
	static std::vector<wxColour> qualitative_colors;
	
	// The following are defined in shp2cnt and should be moved from there.
	//background color -- this is light gray
	static const wxColour backColor;
	// background color -- this is light gray
	static const wxColour darkColor;
	// color of text, frames, points -- this is dark cherry
	static const wxColour textColor;
	// outliers color (also used for regression, etc.) -- blue
	static const wxColour outliers_colour;
	// envelope color (also used for regression, etc.) -- red
	static const wxColour envelope_colour;
	
	// Template Canvas shared by Map, Scatterplot, PCP, etc
	static const int default_virtual_screen_marg_left = 20;
	static const int default_virtual_screen_marg_right = 20;
	static const int default_virtual_screen_marg_top = 20;
	static const int default_virtual_screen_marg_bottom = 20;
	static const int shps_min_width = 100;
	static const int shps_min_height = 100;
	static const int shps_max_width = 6000;
	static const int shps_max_height = 6000;
	static const int shps_max_area = 10000000; // 10 million or 3162 squared
	
	static const wxColour selectable_outline_color; // black
	static const wxColour selectable_fill_color; // forest green
	static const wxColour highlight_color; // yellow
	static const wxColour canvas_background_color; // white
	static const wxColour legend_background_color; // white
	
	// Map
	static const wxSize map_default_size;
	static const int map_default_legend_width;
	// this is a light forest green
	static const wxColour map_default_fill_colour;
	static const wxColour map_default_outline_colour;
	static const int map_default_outline_width = 1;
	static const wxColour map_default_highlight_colour;
	
	// Map Movie
	static const wxColour map_movie_default_fill_colour;
	static const wxColour map_movie_default_highlight_colour;
	
	// Histogram
	static const wxSize hist_default_size;
	
	// Table
	static const wxString new_table_frame_title;
	static const wxString table_frame_title;
	static const wxSize table_default_size;
	static const wxColour table_no_edit_color;
	
	// Scatterplot
	static const wxSize scatterplot_default_size;
	static const wxColour scatterplot_scale_color; // black
	static const wxColour scatterplot_regression_color; // purple
	static const wxColour scatterplot_regression_selected_color; // red
	static const wxColour scatterplot_regression_excluded_color; // blue
	static const wxColour scatterplot_origin_axes_color; // grey
	static wxPen* scatterplot_reg_pen;
	static wxPen* scatterplot_reg_selected_pen;
	static wxPen* scatterplot_reg_excluded_pen;
	static wxPen* scatterplot_scale_pen;
	static wxPen* scatterplot_origin_axes_pen;

	// Bubble Chart
	static const wxSize bubble_chart_default_size;
	static const int bubble_chart_default_legend_width;
	
	// 3D Plot
	static const wxColour three_d_plot_default_highlight_colour;
	static const wxColour three_d_plot_default_point_colour;
	static const wxColour three_d_plot_default_background_colour;
	static const wxSize three_d_default_size;
	
	// Boxplot
	static const wxSize boxplot_default_size;
	static const wxColour boxplot_point_color;
	static const wxColour boxplot_median_color;
	static const wxColour boxplot_mean_point_color;
	static const wxColour boxplot_q1q2q3_color;
	
	// PCP (Parallel Coordinate Plot)
	static const wxSize pcp_default_size;
	static const wxColour pcp_line_color;
	static const wxColour pcp_horiz_line_color;
	
	// Conditional View
	static const wxSize cond_view_default_size;

	// Category Classification
	static const wxSize cat_classif_default_size;
	
	// General Global Constants
	static const int FileNameLen = 512; // max length of file names
	static const int RealWidth = 19;    // default width of output for reals
	static const int ShpHeaderSize = 50; // size of the header record in Shapefile
	static const int ShpObjIdLen = 20;    // length of the ID of shape object
};

#endif
