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

#ifndef __GEODA_CENTER_CARTOGRAM_NEW_VIEW_H__
#define __GEODA_CENTER_CARTOGRAM_NEW_VIEW_H__

#include <vector>
#include <wx/thread.h>
#include "../ShapeOperations/DorlingCartogram.h"
#include "CatClassification.h"
#include "CatClassifStateObserver.h"
#include "../TemplateCanvas.h"
#include "../TemplateLegend.h"
#include "../TemplateFrame.h"
#include "../GenUtils.h"

class CatClassifState;
class CartogramNewFrame;
class CartogramNewCanvas;
class CartogramNewLegend;
class DbfGridTableBase;
class GalWeight;
typedef boost::multi_array<double, 2> d_array_type;

class DorlingCartWorkerThread : public wxThread
{
public:
	DorlingCartWorkerThread(int iters, DorlingCartogram* cart,
							wxMutex* worker_list_mutex,
							wxCondition* worker_list_empty_cond,
							std::list<wxThread*> *worker_list,
							int thread_id);
	virtual ~DorlingCartWorkerThread();
	virtual void* Entry();  // thread execution starts here
	
	int thread_id;
	
	int iters;
	DorlingCartogram* cart;
	
	wxMutex* worker_list_mutex;
	wxCondition* worker_list_empty_cond;
	std::list<wxThread*> *worker_list;
};

class CartogramNewCanvas : public TemplateCanvas, public CatClassifStateObserver
{
	DECLARE_CLASS(CartogramNewCanvas)
public:
	
	CartogramNewCanvas(wxWindow *parent, TemplateFrame* t_frame,
					   Project* project,
					   const std::vector<GeoDaVarInfo>& var_info,
					   const std::vector<int>& col_ids,
					   const wxPoint& pos = wxDefaultPosition,
					   const wxSize& size = wxDefaultSize);
	virtual ~CartogramNewCanvas();
	virtual void DisplayRightClickMenu(const wxPoint& pos);
	virtual void AddTimeVariantOptionsToMenu(wxMenu* menu);
	virtual wxString GetCategoriesTitle();
	virtual wxString GetCanvasTitle();
	virtual wxString GetNameWithTime(int var);
	virtual void NewCustomCatClassif();
	virtual void ChangeThemeType(
						CatClassification::CatClassifType new_cat_theme,
						const wxString& custom_classif_title = wxEmptyString);
	virtual void update(CatClassifState* o);
	virtual void OnSaveCategories();
	virtual void SetCheckMarks(wxMenu* menu);
	virtual void TitleOrTimeChange();
	
protected:
	virtual void PopulateCanvas();
	virtual void VarInfoAttributeChange();
	virtual void CreateAndUpdateCategories();

public:
	virtual void TimeSyncVariableToggle(int var_index);
	CatClassifDef cat_classif_def;
	CatClassification::CatClassifType GetCcType();
	
protected:
	Project* project;
	DbfGridTableBase* grid_base;
	HighlightState* highlight_state;
	CatClassifState* custom_classif_state;
	
	int num_obs;
	int num_time_vals; // current number of valid variable combos
	int num_cats; // current number of categories
	int ref_var_index;
	std::vector<GeoDa::dbl_int_pair_vec_type> cat_var_sorted;
	std::vector<GeoDaVarInfo> var_info;
	std::vector<d_array_type> data;
	bool is_any_time_variant;
	bool is_any_sync_with_global_time;
	std::vector<bool> map_valid;
	std::vector<wxString> map_error_message;

	// Data and functions for individual cartogram maps
	
	void ImproveAll(double max_seconds, int max_iters);
	int EstItersGivenTime(double max_seconds);
	double EstSecondsGivenIters(int max_iters);
	bool realtime_updates;
	
	int num_cpus; // number of cpu cores for multi-threading
	CartNbrInfo* cart_nbr_info;
	// array of pointers to Dorling Cartograms
	// Note: this array corresponds to actual time.
	// so, if variable is time invariant, then only one val
	// otherwise, the actual actual time values are allocated
	std::vector<DorlingCartogram*> carts;
	double secs_per_iter; // assumed to be about the same for every time
	// current number of improvement iterations done so far.
	std::vector<int> num_improvement_iters;
	int GetCurNumCartTms();
	int GetNumBatches();
	GeoDa::dbl_int_pair_vec_type improve_table;
	
public:
	void CartogramImproveLevel(int level);
	void UpdateImproveLevelTable();
	
protected:
	bool full_map_redraw_needed;
	
	GalWeight* gal_weight;
	
	static const int RAD_VAR; // circle size variable
	static const int THM_VAR; // circle color variable
	bool all_init;
	
	virtual void UpdateStatusBar();
		
	DECLARE_EVENT_TABLE()
};

class CartogramNewLegend : public TemplateLegend {
public:
	CartogramNewLegend(wxWindow *parent, TemplateCanvas* template_canvas,
				 const wxPoint& pos, const wxSize& size);
	virtual ~CartogramNewLegend();
};

class CartogramNewFrame : public TemplateFrame {
   DECLARE_CLASS(CartogramNewFrame)
public:
    CartogramNewFrame(wxFrame *parent, Project* project,
					  const std::vector<GeoDaVarInfo>& var_info,
					  const std::vector<int>& col_ids,
					  const wxString& title = "Cartogram",
					  const wxPoint& pos = wxDefaultPosition,
					  const wxSize& size = wxDefaultSize,
					  const long style = wxDEFAULT_FRAME_STYLE);
    virtual ~CartogramNewFrame();

    void OnActivate(wxActivateEvent& event);
    virtual void MapMenus();
    virtual void UpdateOptionMenuItems();
    virtual void UpdateContextMenuItems(wxMenu* menu);
	
	/** Implementation of FramesManagerObserver interface */
	virtual void update(FramesManager* o);

	virtual void UpdateTitle();
	
	virtual void OnNewCustomCatClassifA();
	virtual void OnCustomCatClassifA(const wxString& cc_title);
	void OnThemeless(wxCommandEvent& event);
	void OnQuantile(wxCommandEvent& event);
	void OnPercentile(wxCommandEvent& event);
	void OnHinge15(wxCommandEvent& event);
	void OnHinge30(wxCommandEvent& event);
	void OnStdDevMap(wxCommandEvent& event);
	void OnUniqueValues(wxCommandEvent& event);
	void OnNaturalBreaks(wxCommandEvent& event);
	void OnEqualIntervals(wxCommandEvent& event);
	void OnSaveCategories(wxCommandEvent& event);
	void CartogramImproveLevel(int level);
	
protected:
	void ChangeThemeType(CatClassification::CatClassifType new_cat_theme,
						 const wxString& custom_classif_title = wxEmptyString);
		
    DECLARE_EVENT_TABLE()
};


#endif
