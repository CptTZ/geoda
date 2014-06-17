/**
 * GeoDa TM, Copyright (C) 2011-2014 by Luc Anselin - all rights reserved
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

#ifndef __GEODA_CENTER_CONDITIONAL_SCATTER_PLOT_VIEW_H__
#define __GEODA_CENTER_CONDITIONAL_SCATTER_PLOT_VIEW_H__

#include "../GenUtils.h"
#include "ConditionalNewView.h"

class ConditionalScatterPlotFrame;
class ConditionalScatterPlotCanvas;

typedef boost::multi_array<SampleStatistics, 2> stats_array_type;
typedef boost::multi_array<SimpleLinearRegression, 2> slr_array_type;
typedef boost::multi_array<GdaPolyLine, 2> polyline_array_type;

class ConditionalScatterPlotCanvas : public ConditionalNewCanvas {
	DECLARE_CLASS(ConditionalScatterPlotCanvas)
public:
	
	ConditionalScatterPlotCanvas(wxWindow *parent, TemplateFrame* t_frame,
						 Project* project,
						 const std::vector<GeoDaVarInfo>& var_info,
						 const std::vector<int>& col_ids,
						 const wxPoint& pos = wxDefaultPosition,
						 const wxSize& size = wxDefaultSize);
	virtual ~ConditionalScatterPlotCanvas();
	virtual void DisplayRightClickMenu(const wxPoint& pos);
	virtual wxString GetCanvasTitle();
	
	virtual void SetCheckMarks(wxMenu* menu);

	virtual void ResizeSelectableShps(int virtual_scrn_w = 0,
									  int virtual_scrn_h = 0);
	
protected:
	virtual void PopulateCanvas();
	virtual void CalcCellsRegression();
	
public:
	static void CalcRegressionLine(GdaPolyLine& reg_line, double& slope,
								   bool& infinite_slope,
								   bool& regression_defined,
								   wxRealPoint& reg_a, wxRealPoint& reg_b,
								   double& cc_degs_of_rot,
								   const AxisScale& axis_scale_x,
								   const AxisScale& axis_scale_y,
								   const SimpleLinearRegression& reg,
								   const wxPen& pen);
	static double RegLineToDegCCFromHoriz(double a_x, double a_y,
										  double b_x, double b_y);
	
	virtual void TimeSyncVariableToggle(int var_index);
	void DisplayAxesScaleValues(bool display);
	void DisplaySlopeValues(bool display);
	bool IsDisplayAxesScaleValues();
	bool IsDisplaySlopeValues();
	/// Override from TemplateCanvas
	virtual void SetSelectableFillColor(wxColour color);
	/// Override from TemplateCanvas
	virtual void SetSelectableOutlineColor(wxColour color);	
	
protected:
	bool full_map_redraw_needed;
	std::vector<double> X;
	std::vector<double> Y;
	
	static const int IND_VAR; // scatter plot x-axis
	static const int DEP_VAR; // scatter plot y-axis

	AxisScale axis_scale_x;
	AxisScale axis_scale_y;
	
	stats_array_type stats_x;
	stats_array_type stats_y;
	slr_array_type regression;
	polyline_array_type reg_line;
	
	bool display_axes_scale_values;
	bool display_slope_values;
	
	virtual void UpdateStatusBar();
	
	DECLARE_EVENT_TABLE()
};


class ConditionalScatterPlotFrame : public ConditionalNewFrame {
	DECLARE_CLASS(ConditionalScatterPlotFrame)
public:
    ConditionalScatterPlotFrame(wxFrame *parent, Project* project,
						const std::vector<GeoDaVarInfo>& var_info,
						const std::vector<int>& col_ids,
						const wxString& title = "Conditional Map",
						const wxPoint& pos = wxDefaultPosition,
						const wxSize& size = wxDefaultSize,
						const long style = wxDEFAULT_FRAME_STYLE);
    virtual ~ConditionalScatterPlotFrame();
	
    void OnActivate(wxActivateEvent& event);
    virtual void MapMenus();
    virtual void UpdateOptionMenuItems();
    virtual void UpdateContextMenuItems(wxMenu* menu);
	
	/** Implementation of TimeStateObserver interface */
	virtual void update(TimeState* o);
	
	void OnDisplayAxesScaleValues(wxCommandEvent& event);
	void OnDisplaySlopeValues(wxCommandEvent& event);
	
    DECLARE_EVENT_TABLE()
};

#endif
