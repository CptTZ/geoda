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

#include <boost/foreach.hpp>
#include <boost/random.hpp>
#include <boost/random/uniform_01.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/colourdata.h>
#include <wx/colordlg.h>
#include <wx/filename.h>
#include <wx/textdlg.h>
#include <wx/valnum.h>
#include <wx/valtext.h>
#include <wx/xrc/xmlres.h>
#include "../FramesManager.h"
#include "../GdaConst.h"
#include "../GeneralWxUtils.h"
#include "../Project.h"
#include "../SaveButtonManager.h"
#include "../DataViewer/TableInterface.h"
#include "../DataViewer/TableState.h"
#include "../DataViewer/TimeState.h"
#include "../Explore/CatClassifState.h"
#include "../Generic/HighlightState.h"
#include "../ShapeOperations/ShapeUtils.h"
#include "../logger.h"
#include "CatClassifDlg.h"


BEGIN_EVENT_TABLE(CatClassifHistCanvas, TemplateCanvas)
	EVT_PAINT(TemplateCanvas::OnPaint)
	EVT_ERASE_BACKGROUND(TemplateCanvas::OnEraseBackground)
	EVT_MOUSE_EVENTS(TemplateCanvas::OnMouseEvent)
	EVT_MOUSE_CAPTURE_LOST(TemplateCanvas::OnMouseCaptureLostEvent)
END_EVENT_TABLE()

const int CatClassifHistCanvas::max_intervals = 10;
const int CatClassifHistCanvas::default_intervals = 6;
const double CatClassifHistCanvas::default_min = 0;
const double CatClassifHistCanvas::default_max = 1;
const double CatClassifHistCanvas::left_pad_const = 0;
const double CatClassifHistCanvas::right_pad_const = 0;
const double CatClassifHistCanvas::interval_width_const = 10;
const double CatClassifHistCanvas::interval_gap_const = 0;

CatClassifHistCanvas::CatClassifHistCanvas(wxWindow *parent,
								   TemplateFrame* t_frame,
								   Project* project_s,
								   const wxPoint& pos, const wxSize& size)
: TemplateCanvas(parent, pos, size, false, true),
project(project_s), num_obs(project_s->GetNumRecords()),
highlight_state(project_s->GetHighlightState()),
y_axis(0), data(0), default_data(project_s->GetNumRecords()),
breaks(0), default_breaks(default_intervals-1),
colors(0), default_colors(default_intervals)
{
	using namespace Shapefile;
	LOG_MSG("Entering CatClassifHistCanvas::CatClassifHistCanvas");
	template_frame = t_frame;

	cur_intervals = default_intervals;
	
	InitUniformData(default_data, default_min, default_max);
	// equal spacing between default_min and default_max
	for (int i=0; i<default_intervals-1; i++) {
		default_breaks[i] = ((double) i+1)/((double) default_intervals);
		default_breaks[i] *= (default_max - default_min);
		default_breaks[i] -= default_min;
	}
	CatClassification::PickColorSet(default_colors,
									CatClassification::diverging_color_scheme,
									default_intervals, false);
	data = &default_data;
	breaks = &default_breaks;
	colors = &default_colors;
	
	obs_id_to_ival.resize(num_obs);
	
	InitIntervals();
	
	highlight_color = GdaConst::highlight_color;
	
	fixed_aspect_ratio_mode = false;
	use_category_brushes = false;
	selectable_shps_type = rectangles;
	
	PopulateCanvas();
	
	highlight_state->registerObserver(this);
	SetBackgroundStyle(wxBG_STYLE_CUSTOM);  // default style
	LOG_MSG("Exiting CatClassifHistCanvas::CatClassifHistCanvas");
}

CatClassifHistCanvas::~CatClassifHistCanvas()
{
	LOG_MSG("Entering CatClassifHistCanvas::~CatClassifHistCanvas");
	highlight_state->removeObserver(this);
	LOG_MSG("Exiting CatClassifHistCanvas::~CatClassifHistCanvas");
}

void CatClassifHistCanvas::DisplayRightClickMenu(const wxPoint& pos)
{
	LOG_MSG("Entering CatClassifHistCanvas::DisplayRightClickMenu");
	// Workaround for right-click not changing window focus in OSX / wxW 3.0
	wxActivateEvent ae(wxEVT_NULL, true, 0, wxActivateEvent::Reason_Mouse);
	((CatClassifFrame*) template_frame)->OnActivate(ae);
	
	wxMenu* optMenu;
	optMenu = wxXmlResource::Get()->
		LoadMenu("ID_CAT_CLASSIF_HIST_VIEW_MENU_OPTIONS");
	SetCheckMarks(optMenu);
	template_frame->UpdateContextMenuItems(optMenu);
	// The position passed in is the mouse position relative to
	// the canvas window, not the parent frame.  This is corrected
	// by adding the position of the canvas relative to its parent
	template_frame->PopupMenu(optMenu,
							  wxPoint(GetPosition().x + pos.x, pos.y));
	template_frame->UpdateOptionMenuItems();
	LOG_MSG("Exiting CatClassifHistCanvas::DisplayRightClickMenu");
}


void CatClassifHistCanvas::SetCheckMarks(wxMenu* menu)
{
	// Update the checkmarks and enable/disable state for the
	// following menu items if they were specified for this particular
	// view in the xrc file.  Items that cannot be enable/disabled,
	// or are not checkable do not appear.
}

void CatClassifHistCanvas::DetermineMouseHoverObjects()
{
	total_hover_obs = 0;
	for (int i=0, iend=selectable_shps.size(); i<iend; i++) {
		if (selectable_shps[i]->pointWithin(sel1)) {
			hover_obs[total_hover_obs++] = i;
			if (total_hover_obs == max_hover_obs) break;
		}
	}
}

// The following function assumes that the set of selectable objects
// all rectangles.
void CatClassifHistCanvas::UpdateSelection(bool shiftdown, bool pointsel)
{
	bool rect_sel = (!pointsel && (brushtype == rectangle));
	
	std::vector<bool>& hs = highlight_state->GetHighlight();
	std::vector<int>& nh = highlight_state->GetNewlyHighlighted();
	std::vector<int>& nuh = highlight_state->GetNewlyUnhighlighted();
	int total_newly_selected = 0;
	int total_newly_unselected = 0;
	
	int total_sel_shps = selectable_shps.size();
	
	wxPoint lower_left;
	wxPoint upper_right;
	if (rect_sel) {
		GenUtils::StandardizeRect(sel1, sel2, lower_left, upper_right);
	}
	if (!shiftdown) {
		bool any_selected = false;
		for (int i=0; i<total_sel_shps; i++) {
			GdaRectangle* rec = (GdaRectangle*) selectable_shps[i];
			if ((pointsel && rec->pointWithin(sel1)) ||
				(rect_sel &&
				 GenUtils::RectsIntersect(rec->lower_left, rec->upper_right,
										  lower_left, upper_right))) {
					 //LOG_MSG(wxString::Format("ival %d selected", i));
					 any_selected = true;
					 break;
				 } else {
					 //LOG_MSG(wxString::Format("ival %d not selected", i));
					 //LOG_MSG(wxString::Format(""));
				 }
		}
		if (!any_selected) {
			highlight_state->SetEventType(HighlightState::unhighlight_all);
			highlight_state->notifyObservers();
			return;
		}
	}
	
	for (int i=0; i<total_sel_shps; i++) {
		GdaRectangle* rec = (GdaRectangle*) selectable_shps[i];
		bool selected = ((pointsel && rec->pointWithin(sel1)) ||
						 (rect_sel &&
						  GenUtils::RectsIntersect(rec->lower_left,
												   rec->upper_right,
												   lower_left, upper_right)));
		bool all_sel = (ival_obs_cnt[i] == ival_obs_sel_cnt[i]);
		if (pointsel && all_sel && selected) {
			// unselect all in ival
			for (std::list<int>::iterator it=ival_to_obs_ids[i].begin();
				 it != ival_to_obs_ids[i].end(); it++) {
				nuh[total_newly_unselected++] = (*it);
			}
		} else if (!all_sel && selected) {
			// select currently unselected in ival
			for (std::list<int>::iterator it=ival_to_obs_ids[i].begin();
				 it != ival_to_obs_ids[i].end(); it++) {
				if (hs[*it]) continue;
				nh[total_newly_selected++] = (*it);
			}
		} else if (!selected && !shiftdown) {
			// unselect all selected in ival
			for (std::list<int>::iterator it=ival_to_obs_ids[i].begin();
				 it != ival_to_obs_ids[i].end(); it++) {
				if (!hs[*it]) continue;
				nuh[total_newly_unselected++] = (*it);
			}
		}
	}
	
	if (total_newly_selected > 0 || total_newly_unselected > 0) {
		highlight_state->SetTotalNewlyHighlighted(total_newly_selected);
		highlight_state->SetTotalNewlyUnhighlighted(total_newly_unselected);
		NotifyObservables();
	}
	UpdateStatusBar();
}

void CatClassifHistCanvas::DrawSelectableShapes(wxMemoryDC &dc)
{
	for (int i=0, iend=selectable_shps.size(); i<iend; i++) {
		if (ival_obs_cnt[i] == 0) continue;
		selectable_shps[i]->paintSelf(dc);
	}
}

void CatClassifHistCanvas::DrawHighlightedShapes(wxMemoryDC &dc)
{
	dc.SetPen(wxPen(highlight_color));
	dc.SetBrush(wxBrush(highlight_color, wxBRUSHSTYLE_CROSSDIAG_HATCH));
	
	for (int i=0, iend=selectable_shps.size(); i<iend; i++) {
		if (ival_obs_sel_cnt[i] == 0) continue;
		double s = (((double) ival_obs_sel_cnt[i]) /
					((double) ival_obs_cnt[i]));
		GdaRectangle* rec = (GdaRectangle*) selectable_shps[i];
		dc.DrawRectangle(rec->lower_left.x, rec->lower_left.y,
						 rec->upper_right.x - rec->lower_left.x,
						 (rec->upper_right.y - rec->lower_left.y)*s);
	}	
}

/** Override of TemplateCanvas method. */
void CatClassifHistCanvas::update(HighlightState* o)
{
	LOG_MSG("Entering CatClassifHistCanvas::update");
	
	int total = highlight_state->GetTotalNewlyHighlighted();
	std::vector<int>& nh = highlight_state->GetNewlyHighlighted();
	
	HighlightState::EventType type = highlight_state->GetEventType();
	layer0_valid = false;
	layer1_valid = false;
	layer2_valid = false;
	
	UpdateIvalSelCnts();
	
	Refresh();
	
	LOG_MSG("Entering CatClassifHistCanvas::update");	
}

wxString CatClassifHistCanvas::GetCanvasTitle()
{
	wxString s;
	s << "Category Editor";
	return s;
}

void CatClassifHistCanvas::PopulateCanvas()
{
	LOG_MSG("Entering CatClassifHistCanvas::PopulateCanvas");
	BOOST_FOREACH( GdaShape* shp, background_shps ) { delete shp; }
	background_shps.clear();
	BOOST_FOREACH( GdaShape* shp, selectable_shps ) { delete shp; }
	selectable_shps.clear();
	BOOST_FOREACH( GdaShape* shp, foreground_shps ) { delete shp; }
	foreground_shps.clear();
	
	double x_min = 0;
	double x_max = left_pad_const + right_pad_const
	+ interval_width_const * cur_intervals + 
	+ interval_gap_const * (cur_intervals-1);
	
	// orig_x_pos is the center of each histogram bar
	std::vector<double> orig_x_pos(cur_intervals);
	for (int i=0; i<cur_intervals; i++) {
		orig_x_pos[i] = left_pad_const + interval_width_const/2.0
		+ i * (interval_width_const + interval_gap_const);
	}
	
	shps_orig_xmin = x_min;
	shps_orig_xmax = x_max;
	shps_orig_ymin = 0;
	shps_orig_ymax = max_num_obs_in_ival;
	
	axis_scale_y = AxisScale(0, shps_orig_ymax, 5);
	shps_orig_ymax = axis_scale_y.scale_max;
	y_axis = new GdaAxis("Frequency", axis_scale_y,
						wxRealPoint(0,0), wxRealPoint(0, shps_orig_ymax),
						-9, 0);
	background_shps.push_back(y_axis);

	virtual_screen_marg_top = 25;
	virtual_screen_marg_bottom = 25+32;
	virtual_screen_marg_left = 25+35;
	virtual_screen_marg_right = 25;
	
	selectable_shps.resize(cur_intervals);
	for (int i=0; i<cur_intervals; i++) {
		double x0 = orig_x_pos[i] - interval_width_const/2.0;
		double x1 = orig_x_pos[i] + interval_width_const/2.0;
		double y0 = 0;
		double y1 = ival_obs_cnt[i];
		selectable_shps[i] = new GdaRectangle(wxRealPoint(x0, 0),
											 wxRealPoint(x1, y1));
		selectable_shps[i]->setPen((*colors)[i]);
		selectable_shps[i]->setBrush((*colors)[i]);
		
		if (i<cur_intervals-1) {
			GdaShapeText* brk = 
				new GdaShapeText(GenUtils::DblToStr((*breaks)[i]),
								 *GdaConst::small_font,
								 wxRealPoint(x1, y0), 90,
								 GdaShapeText::right,
								 GdaShapeText::v_center, 0, 10);
			background_shps.push_back(brk);
		}
	}
	
	ResizeSelectableShps();
	
	LOG_MSG("Exiting CatClassifHistCanvas::PopulateCanvas");
}

/** based on data and breaks and cur_intervals,
 populate obs_id_to_ival, ival_to_obs_id, ival_obs_cnt and ival_obs_sel_cnt
 Note: this algorithm is based on the theme == quantile_break_vals case
 in the CatClassification::PopulateCatClassifData method. */ 
void CatClassifHistCanvas::InitIntervals()
{
	std::vector<bool>& hs = highlight_state->GetHighlight();
	
	ival_obs_cnt.resize(cur_intervals);
	ival_obs_sel_cnt.resize(cur_intervals);
	ival_to_obs_ids.clear();
	ival_to_obs_ids.resize(cur_intervals);
	for (int i=0; i<cur_intervals; i++) {
		ival_obs_cnt[i] = 0;
		ival_obs_sel_cnt[i] = 0;
	}
	
	if (cur_intervals == 1) {
		for (int i=0; i<num_obs; i++) {
			obs_id_to_ival[i] = 0;
			ival_to_obs_ids[0].push_front(i);
		}		
		ival_obs_cnt[0] = num_obs;
		ival_obs_sel_cnt[0] = highlight_state->GetTotalHighlighted();
	} else {
		int num_breaks = breaks->size();
		int num_breaks_lower = (num_breaks+1)/2;
		
		double val;
		int ind;
		for (int i=0; i<num_obs; i++) {
			val = (*data)[i].first;
			ind = (*data)[i].second;
			bool found = false;
			int cat = num_breaks; // last cat by default
			for (int j=0; j<num_breaks_lower; j++) {
				if (val < (*breaks)[j]) {
					found = true;
					cat = j;
					break;
				}
			}
			if (!found) {
				for (int j=num_breaks_lower; j<num_breaks; j++) {
					if (val <= (*breaks)[j]) {
						cat = j;
						break;
					}
				}
			}
			// at this point we know that index ind belongs to category cat
			obs_id_to_ival[ind] = cat;
			ival_to_obs_ids[cat].push_front(ind);
			ival_obs_cnt[cat]++;
			if (hs[ind]) ival_obs_sel_cnt[cat]++;
		}
	}
	
	max_num_obs_in_ival = 0;
	for (int i=0; i<cur_intervals; i++) {
		if (ival_obs_cnt[i] > max_num_obs_in_ival) {
			max_num_obs_in_ival = ival_obs_cnt[i];
		}
	}
	
	LOG_MSG("InitIntervals: ");
	LOG_MSG(wxString::Format("max_num_obs_in_ival: %f", max_num_obs_in_ival));
	for (int i=0; i<cur_intervals; i++) {
		LOG_MSG(wxString::Format("ival_obs_cnt[%d] = %d",
								 i, ival_obs_cnt[i]));
	}
}

void CatClassifHistCanvas::UpdateIvalSelCnts()
{
	HighlightState::EventType type = highlight_state->GetEventType();
	if (type == HighlightState::unhighlight_all) {
		for (int i=0; i<cur_intervals; i++) {
			ival_obs_sel_cnt[i] = 0;
		}
	} else if (type == HighlightState::delta) {
		std::vector<int>& nh = highlight_state->GetNewlyHighlighted();
		std::vector<int>& nuh = highlight_state->GetNewlyUnhighlighted();
		int nh_cnt = highlight_state->GetTotalNewlyHighlighted();
		int nuh_cnt = highlight_state->GetTotalNewlyUnhighlighted();
		
		for (int i=0; i<nh_cnt; i++) {
			ival_obs_sel_cnt[obs_id_to_ival[nh[i]]]++;
		}
		for (int i=0; i<nuh_cnt; i++) {
			ival_obs_sel_cnt[obs_id_to_ival[nuh[i]]]--;
		}
	} else if (type == HighlightState::invert) {
		for (int i=0; i<cur_intervals; i++) {
			ival_obs_sel_cnt[i] = 
			ival_obs_cnt[i] - ival_obs_sel_cnt[i];
		}
	}
}

void CatClassifHistCanvas::ChangeAll(Gda::dbl_int_pair_vec_type* new_data,
									 std::vector<double>* new_breaks,
									 std::vector<wxColour>* new_colors)
{
	data = new_data;
	breaks = new_breaks;
	colors = new_colors;
	cur_intervals = breaks->size() + 1;
	InitIntervals();
	PopulateCanvas();
}

void CatClassifHistCanvas::InitRandNormData(Gda::dbl_int_pair_vec_type& rnd)
{
	int n_obs = rnd.size();
	// Mersenne Twister random number generator, randomly seeded
	// with current time in seconds since Jan 1 1970.
	static boost::mt19937 rng(std::time(0));
	boost::normal_distribution<> norm_dist(0.0, 1.0);
	boost::variate_generator<boost::mt19937&,
	boost::normal_distribution<> > X(rng, norm_dist);
	for (int i=0; i<n_obs; i++) {
		rnd[i].first = X();
		rnd[i].second = i;
	}
	std::sort(rnd.begin(), rnd.end(),
			  Gda::dbl_int_pair_cmp_less);
}

void CatClassifHistCanvas::InitUniformData(Gda::dbl_int_pair_vec_type& data,
										   double min, double max)
{
	int n_obs=data.size();
	double dn_obs = (double) n_obs;
	for (int i=0; i<n_obs; ++i) {
		double di = (double) i;
		data[i].first = min + (max-min)*(di/dn_obs);
		data[i].second = i;
	}
}

void CatClassifHistCanvas::UpdateStatusBar()
{
	wxStatusBar* sb = template_frame->GetStatusBar();
	if (!sb) return;
	if (total_hover_obs == 0) {
		sb->SetStatusText("");
		return;
	}
	int ival = hover_obs[0];
	wxString s;
	s << "bin: " << ival+1 << ", range: ";
	if (cur_intervals <= 1) {
		s << "(-inf, inf)";
	} else if (ival == 0) {
		s << "(-inf, " << (*breaks)[ival] << ")";
	} else if (ival == cur_intervals-1 && cur_intervals != 2) {
		s << "(" << (*breaks)[ival-1] << ", inf)";
	} else if (ival == cur_intervals-1 && cur_intervals == 2) {
		s << "[" << (*breaks)[ival-1] << ", inf)";
	} else {
		int num_breaks = cur_intervals-1;
		int num_breaks_lower = (num_breaks+1)/2;
		wxString a;
		wxString b;
		if (ival < num_breaks_lower) {
			a = "[";
			b = ")";
		} else if (ival == num_breaks_lower) {
			a = "[";
			b = "]";
		} else {
			a = "(";
			b = "]";
		}
		s << a << (*breaks)[ival-1] << ", " << (*breaks)[ival] << b;
	}
	s << ", #obs: " << ival_obs_cnt[ival];
	double p = 100.0*((double) ival_obs_cnt[ival])/((double) num_obs);
	s << ", %tot: ";
	s << wxString::Format("%.1f", p);
	s << "%, #sel: " << ival_obs_sel_cnt[ival];
	
	sb->SetStatusText(s);
}

const wxString CatClassifPanel::unif_dist_txt("uniform distribution");
const int CatClassifPanel::max_intervals = 10;
const int CatClassifPanel::default_intervals = 6;
const double CatClassifPanel::default_min = 0;
const double CatClassifPanel::default_max = 1;

typedef std::pair<int, wxWindow*> int_win_pair_type;
typedef std::vector<int_win_pair_type> int_win_pair_vec_type;
bool int_win_pair_less(const int_win_pair_type& ind1,
					   const int_win_pair_type& ind2)
{
	return ind1.first < ind2.first;
}

BEGIN_EVENT_TABLE(CatClassifPanel, wxPanel)
	EVT_CHOICE(XRCID("ID_CUR_CATS_CHOICE"), CatClassifPanel::OnCurCatsChoice)
	EVT_CHOICE(XRCID("ID_BREAKS_CHOICE"), CatClassifPanel::OnBreaksChoice)
	EVT_CHOICE(XRCID("ID_COLOR_SCHEME"), CatClassifPanel::OnColorSchemeChoice)
	EVT_CHOICE(XRCID("ID_NUM_CATS_CHOICE"), CatClassifPanel::OnNumCatsChoice)
	EVT_CHOICE( XRCID("ID_ASSOC_VAR"), CatClassifPanel::OnAssocVarChoice)
	EVT_CHOICE( XRCID("ID_ASSOC_VAR_TM"),
			   CatClassifPanel::OnAssocVarTmChoice)
	EVT_TEXT_ENTER(XRCID("ID_UNIF_DIST_MIN_TXT"),
				   CatClassifPanel::OnUnifDistMinEnter)
	EVT_TEXT_ENTER(XRCID("ID_UNIF_DIST_MAX_TXT"),
				   CatClassifPanel::OnUnifDistMaxEnter)
	EVT_CHECKBOX(XRCID("ID_AUTO_LABELS_CB"),
				 CatClassifPanel::OnAutomaticLabelsCb)
	EVT_SLIDER(XRCID("ID_BRK_SLIDER"), CatClassifPanel::OnBrkSlider)
	EVT_SCROLL_THUMBRELEASE(CatClassifPanel::OnScrollThumbRelease)
	EVT_TEXT(XRCID("ID_CAT_TXT"), CatClassifPanel::OnCategoryTitleText)
	EVT_RADIOBUTTON(XRCID("ID_BRK_RAD"), CatClassifPanel::OnBrkRad)	
	EVT_TEXT_ENTER(XRCID("ID_BRK_TXT"), CatClassifPanel::OnBrkTxtEnter)
	EVT_BUTTON(XRCID("ID_CHANGE_TITLE"),
			   CatClassifPanel::OnButtonChangeTitle)
	EVT_BUTTON(XRCID("wxID_NEW"), CatClassifPanel::OnButtonNew)
	EVT_BUTTON(XRCID("wxID_DELETE"), CatClassifPanel::OnButtonDelete)
	EVT_BUTTON(XRCID("wxID_CLOSE"), CatClassifPanel::OnButtonClose)
END_EVENT_TABLE()

CatClassifPanel::CatClassifPanel(Project* project_s,
								 CatClassifHistCanvas* hist_canvas_s,
								 wxWindow* parent,
								 wxChoice* preview_var_choice_s,
								 wxChoice* preview_var_tm_choice_s,
								 wxCheckBox* sync_vars_chk_s,
								 wxWindowID id,
								 const wxPoint& pos, const wxSize& size,
								 long style)
: project(project_s), table_int(project_s->GetTableInt()),
hist_canvas(hist_canvas_s),
cat_classif_manager(project_s->GetCatClassifManager()), cc_state(0),
table_state(project_s->GetTableState()),
data(project_s->GetNumRecords()),
preview_data(project_s->GetNumRecords()),
num_obs(project_s->GetNumRecords()),
assoc_var_choice(0), assoc_var_tm_choice(0),
preview_var_choice(preview_var_choice_s),
preview_var_tm_choice(preview_var_tm_choice_s),
sync_vars_chk(sync_vars_chk_s),
unif_dist_mode(true),
all_init(false)
{
	using namespace std;
	SetParent(parent);
	
	CatClassifHistCanvas::InitUniformData(data, cc_data.uniform_dist_min,
										  cc_data.uniform_dist_max);
	CatClassifHistCanvas::InitUniformData(preview_data,
										  cc_data.uniform_dist_min,
										  cc_data.uniform_dist_max);
	cc_data.cat_classif_type = CatClassification::custom;
	cc_data.break_vals_type = CatClassification::custom_break_vals;
	cc_data.color_scheme = CatClassification::diverging_color_scheme;
	CatClassification::ChangeNumCats(default_intervals, cc_data);
	
	//Begin Creating Controls
	
	wxXmlResource::Get()->LoadPanel(this, GetParent(), "ID_CAT_CLASSIF");
	
	num_cats_choice = wxDynamicCast(FindWindow(XRCID("ID_NUM_CATS_CHOICE")),
									wxChoice);
	cur_cats_choice = wxDynamicCast(FindWindow(XRCID("ID_CUR_CATS_CHOICE")),
									wxChoice);
	breaks_choice = wxDynamicCast(FindWindow(XRCID("ID_BREAKS_CHOICE")),
								  wxChoice);

	change_title_button =
		wxDynamicCast(FindWindow(XRCID("ID_CHANGE_TITLE")), wxButton);
	delete_button =	wxDynamicCast(FindWindow(XRCID("wxID_DELETE")), wxButton);
	min_lbl = wxDynamicCast(FindWindow(XRCID("ID_MIN_LBL")), wxStaticText);
	max_lbl = wxDynamicCast(FindWindow(XRCID("ID_MAX_LBL")), wxStaticText);

	auto_labels_cb = wxDynamicCast(FindWindow(XRCID("ID_AUTO_LABELS_CB")),
								  wxCheckBox);
	auto_labels_cb->SetValue(cc_data.automatic_labels);
	
	color_scheme = wxDynamicCast(FindWindow(XRCID("ID_COLOR_SCHEME")),
								 wxChoice);
	color_scheme->SetSelection(1);
	assoc_var_choice = wxDynamicCast(FindWindow(XRCID("ID_ASSOC_VAR")),
									   wxChoice);
	assoc_var_choice->Clear();
	assoc_var_choice->Append(unif_dist_txt);
	assoc_var_choice->SetSelection(0);
	assoc_var_tm_choice = 
		wxDynamicCast(FindWindow(XRCID("ID_ASSOC_VAR_TM")), wxChoice);
	assoc_var_tm_choice->Show(project->GetTableInt()->IsTimeVariant());

	preview_var_choice->Clear();
	preview_var_choice->Append(unif_dist_txt);
	preview_var_choice->SetSelection(0);
	preview_var_tm_choice->Show(project->GetTableInt()->IsTimeVariant());

	unif_dist_min_lbl = wxDynamicCast(FindWindow(XRCID("ID_UNIF_DIST_MIN_LBL")),
									  wxStaticText);
	unif_dist_min_txt = wxDynamicCast(FindWindow(XRCID("ID_UNIF_DIST_MIN_TXT")),
									   wxTextCtrl);
	unif_dist_min_txt->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
	unif_dist_min_txt->Bind(wxEVT_KILL_FOCUS,
							&CatClassifPanel::OnUnifDistMinKillFocus,
							this);
	
	unif_dist_max_lbl = wxDynamicCast(FindWindow(XRCID("ID_UNIF_DIST_MAX_LBL")),
									  wxStaticText);
	unif_dist_max_txt = wxDynamicCast(FindWindow(XRCID("ID_UNIF_DIST_MAX_TXT")),
									  wxTextCtrl);	
	unif_dist_max_txt->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
	unif_dist_max_txt->Bind(wxEVT_KILL_FOCUS,
							&CatClassifPanel::OnUnifDistMaxKillFocus,
							this);
	
	cc_data.uniform_dist_min = default_min;
	cc_data.uniform_dist_max = default_max;
	ShowUnifDistMinMax(true);
	SetUnifDistMinMaxTxt(cc_data.uniform_dist_min, cc_data.uniform_dist_max);
	
	brk_slider = wxDynamicCast(FindWindow(XRCID("ID_BRK_SLIDER")), wxSlider);
	
	int_win_pair_vec_type cat_color_button_srt_vec;
	int_win_pair_vec_type cat_title_txt_srt_vec;
	int_win_pair_vec_type brk_rad_srt_vec;
	int_win_pair_vec_type brk_lbl_srt_vec;
	int_win_pair_vec_type brk_txt_srt_vec;
	
	wxWindowList& win_list = GetChildren();
	for (wxWindowList::iterator it = win_list.begin(); it != win_list.end();
		 it++) {
		if ((*it)->GetId() == XRCID("ID_CAT_BUT")) {
			cat_color_button_srt_vec.push_back(make_pair((*it)->GetPosition().y,
														 (*it)));
		} else if ((*it)->GetId() == XRCID("ID_CAT_TXT")) {
			cat_title_txt_srt_vec.push_back(make_pair((*it)->GetPosition().y,
													  (*it)));
		} else if ((*it)->GetId() == XRCID("ID_BRK_RAD")) {
			brk_rad_srt_vec.push_back(make_pair((*it)->GetPosition().y, (*it)));
		} else if ((*it)->GetId() == XRCID("ID_BRK_LBL")) {
			brk_lbl_srt_vec.push_back(make_pair((*it)->GetPosition().y, (*it)));
		} else if ((*it)->GetId() == XRCID("ID_BRK_TXT")) {
			brk_txt_srt_vec.push_back(make_pair((*it)->GetPosition().y, (*it)));
		}
	}
		
	std::sort(cat_color_button_srt_vec.begin(), cat_color_button_srt_vec.end(),
			  int_win_pair_less);
	cat_color_button.resize(cat_color_button_srt_vec.size());
	for (int i=0, iend=cat_color_button_srt_vec.size(); i<iend; i++) {
		cat_color_button[i] = 
			(wxStaticBitmap*) cat_color_button_srt_vec[i].second;
		cat_color_button[i]->Bind(wxEVT_LEFT_UP,
								&CatClassifPanel::OnCategoryColorButton, this);
	}
	
	std::sort(cat_title_txt_srt_vec.begin(), cat_title_txt_srt_vec.end(),
			  int_win_pair_less);
	cat_title_txt.resize(cat_title_txt_srt_vec.size());
	for (int i=0, iend=cat_title_txt_srt_vec.size(); i<iend; i++) {
		cat_title_txt[i] = (wxTextCtrl*) cat_title_txt_srt_vec[i].second;
	}
	for (int i=0; i<cc_data.num_cats; i++) {
		cc_data.names[i] = cat_title_txt[i]->GetValue();
	}
	
	std::sort(brk_rad_srt_vec.begin(), brk_rad_srt_vec.end(),
			  int_win_pair_less);
	brk_rad.resize(brk_rad_srt_vec.size());
	for (int i=0, iend=brk_rad_srt_vec.size(); i<iend; i++) {
		brk_rad[i] = (wxRadioButton*) brk_rad_srt_vec[i].second;
	}	
	
	std::sort(brk_lbl_srt_vec.begin(), brk_lbl_srt_vec.end(),
			  int_win_pair_less);
	brk_lbl.resize(brk_lbl_srt_vec.size());
	for (int i=0, iend=brk_lbl_srt_vec.size(); i<iend; i++) {
		brk_lbl[i] = (wxStaticText*) brk_lbl_srt_vec[i].second;
		wxString s;
		s << "break " << i+1 << ":";
		brk_lbl[i]->SetLabelText(s);
	}
	
	std::sort(brk_txt_srt_vec.begin(), brk_txt_srt_vec.end(),
			  int_win_pair_less);
	brk_txt.resize(brk_txt_srt_vec.size());
	for (int i=0, iend=brk_txt_srt_vec.size(); i<iend; i++) {
		brk_txt[i] = (wxTextCtrl*) brk_txt_srt_vec[i].second;
		brk_txt[i]->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
		//brk_txt[i]->SetValidator(wxFloatingPointValidator<double> validator);
		brk_txt[i]->Bind(wxEVT_KILL_FOCUS, &CatClassifPanel::OnKillFocusEvent,
						 this);
	}
	brk_slider->SetRange(0, 1000);
	
	//End Creating Controls
	//LOG_MSG(cc_data.ToStr());
	all_init = true;
	InitCurCatsChoices();
	InitAssocVarChoices();
	InitPreviewVarChoices();
	ResetValuesToDefault();
	EnableControls(false);
	ShowNumCategories(default_intervals);
	
	wxString cc_str_sel = cur_cats_choice->GetStringSelection();
	cc_state = cat_classif_manager->FindClassifState(cc_str_sel);
	if (cc_state) {
		cc_data = cc_state->GetCatClassif();
		SetSyncVars(true);
		InitFromCCData();
		EnableControls(true);
	}
	
	table_state->registerObserver(this);
    GetSizer()->Fit(this);
    GetSizer()->SetSizeHints(this);
    Centre();
}

CatClassifPanel::~CatClassifPanel()
{
	table_state->removeObserver(this);
}

CatClassifState* CatClassifPanel::PromptNew(const CatClassifDef& ccd,
											const wxString& suggested_title,
											const wxString& field_name,
											int field_tm)
{
	if (!all_init) return 0;
	wxString msg;
	msg << "New Custom Categories Title:";
	wxString new_title = (suggested_title.IsEmpty() ?
						  GetDefaultTitle(field_name, field_tm) :
						  suggested_title);
	bool retry = true;
	bool success = false;
	while (retry) {
		wxTextEntryDialog dlg(this, msg, "New Categories Title");
		dlg.SetValue(new_title);
		if (dlg.ShowModal() == wxID_OK) {
			new_title = dlg.GetValue();
			new_title.Trim(false);
			new_title.Trim(true);
			if (new_title.IsEmpty()) {
				retry = false;
			} else if (IsDuplicateTitle(new_title)) {
				wxString es;
				es << "Categories title \"" << new_title << "\" already ";
				es << "exists. Please choose a different title.";
				wxMessageDialog ed(NULL, es, "Error", wxOK | wxICON_ERROR);
				ed.ShowModal();
			} else {
				success = true;
				cc_data = ccd;
				cc_data.title = new_title;
				CatClassification::CatClassifTypeToBreakValsType(
													cc_data.cat_classif_type);
				cc_data.cat_classif_type = CatClassification::custom;
				int f_sel = assoc_var_choice->FindString(field_name);
				if (f_sel != wxNOT_FOUND) {
					assoc_var_choice->SetSelection(f_sel);
					if (table_int->IsColTimeVariant(field_name)) {
						assoc_var_tm_choice->SetSelection(field_tm);
					}
				}
				cc_state = cat_classif_manager->CreateNewClassifState(cc_data);
				SetSyncVars(true);
				InitFromCCData();
				cur_cats_choice->Append(new_title);
				cur_cats_choice->SetSelection(cur_cats_choice->GetCount()-1);
				EnableControls(true);
				retry = false;
			}
		} else {
			retry = false;
		}
	}
	return success ? cc_state : 0;
}

/** A new Custom Category was selected.  Copy cc_data from cc_state and
 re-initialize interface. */
void CatClassifPanel::OnCurCatsChoice(wxCommandEvent& event)
{
	if (!all_init) return;
	wxString cc_str_sel = cur_cats_choice->GetStringSelection();
	cc_state = cat_classif_manager->FindClassifState(cc_str_sel);
	if (!cc_state) return;
	cc_data = cc_state->GetCatClassif();
	SetSyncVars(true);
	InitFromCCData();
	UpdateCCState();
	EnableControls(true);
}

void CatClassifPanel::OnBreaksChoice(wxCommandEvent& event)
{
	if (!all_init) return;
	CatClassification::BreakValsType bv_type = GetBreakValsTypeChoice();
	cc_data.break_vals_type = bv_type;
	InitFromCCData();
	UpdateCCState();
}

void CatClassifPanel::OnColorSchemeChoice(wxCommandEvent& event)
{
	if (!all_init) return;
	cc_data.color_scheme = GetColorSchemeChoice();
	if (cc_data.color_scheme != CatClassification::custom_color_scheme) {
		CatClassification::PickColorSet(cc_data.colors,
										cc_data.color_scheme,
										cc_data.num_cats, false);
		for (size_t i=0; i<cc_data.colors.size(); i++) {
			cat_color_button[i]->SetBackgroundColour(cc_data.colors[i]);
		}
	}
	InitFromCCData();
	UpdateCCState();
}

void CatClassifPanel::OnNumCatsChoice(wxCommandEvent& event)
{
	if (!all_init) return;
	int new_num_cats = GetNumCats();
	if (new_num_cats == cc_data.num_cats ||
		new_num_cats < 1 || new_num_cats > max_intervals) return;
	
	CatClassification::BreakValsType new_cat_typ = GetBreakValsTypeChoice();
	if (new_cat_typ == CatClassification::hinge_15_break_vals ||
		new_cat_typ == CatClassification::hinge_30_break_vals ||
		new_cat_typ == CatClassification::stddev_break_vals ||
		new_cat_typ == CatClassification::percentile_break_vals ||
		new_cat_typ == CatClassification::unique_values_break_vals ||
		new_cat_typ == CatClassification::no_theme_break_vals)
	{
		cc_data.break_vals_type = CatClassification::custom_break_vals;
	} else {
		cc_data.break_vals_type = new_cat_typ;
	}
	cc_data.num_cats = new_num_cats;
	InitFromCCData();
	UpdateCCState();
}

void CatClassifPanel::OnAssocVarChoice(wxCommandEvent& ev)
{
	LOG_MSG("Entering CatClassifPanel::OnAssocVarChoice");
	wxString cur_fc_str = assoc_var_choice->GetStringSelection();
	bool is_tm_var = table_int->IsColTimeVariant(cur_fc_str);
	assoc_var_tm_choice->Enable(is_tm_var);
	if (is_tm_var && assoc_var_tm_choice->GetSelection() == wxNOT_FOUND) {
		assoc_var_tm_choice->SetSelection(0);
	}
	if (cc_state) {
		cc_state->GetCatClassif().assoc_db_fld_name = GetAssocDbFldNm();
		cc_data.assoc_db_fld_name = GetAssocDbFldNm();
	}
	InitFromCCData();
	UpdateCCState();
	LOG_MSG("Exiting CatClassifPanel::OnAssocVarChoice");
}

void CatClassifPanel::OnAssocVarTmChoice(wxCommandEvent& ev)
{
	LOG_MSG("Entering CatClassifPanel::OnAssocVarTmChoice");
	if (cc_state) {
		cc_state->GetCatClassif().assoc_db_fld_name = GetAssocDbFldNm();
		cc_data.assoc_db_fld_name = GetAssocDbFldNm();
	}
	InitFromCCData();
	UpdateCCState();
	LOG_MSG("Exiting CatClassifPanel::OnAssocVarTmChoice");
}


void CatClassifPanel::OnPreviewVarChoice(wxCommandEvent& ev)
{
	LOG_MSG("Entering CatClassifPanel::OnPreviewVarChoice");
	bool preview_unif_dist_mode = (preview_var_choice->GetSelection() == 0);
	if (!preview_unif_dist_mode) {
		wxString cur_fc_str = preview_var_choice->GetStringSelection();
		bool is_tm_var = table_int->IsColTimeVariant(cur_fc_str);
		preview_var_tm_choice->Enable(is_tm_var);
		if (is_tm_var && preview_var_tm_choice->GetSelection() == wxNOT_FOUND) {
			preview_var_tm_choice->SetSelection(0);
		}
		if (GetPreviewDbFldNm() == "") {
			SetSyncVars(true);
			InitFromCCData();
			return;
		}
		// change preview_data to new choice.  We know the choice cannot
		// be "uniform distribution" if the user chose it because this option
		// is removed from the list whenever IsSyncVars() is false.
		int col, tm;
		table_int->DbColNmToColAndTm(GetPreviewDbFldNm(), col, tm);
		std::vector<double> dd;
		table_int->GetColData(col, tm, dd);
		for (size_t i=0, sz=dd.size(); i<sz; ++i) {
			preview_data[i].first = dd[i];
			preview_data[i].second = i;
		}
	} else {
		double prev_min = data[0].first;
		double prev_max = data[data.size()-1].first;
		if (prev_min > prev_max) {
			double t = prev_min;
			prev_min = prev_max;
			prev_max = t;
		}
		if (prev_min == prev_max) {
			if (prev_min == 0) {
				prev_max = 1;
			} else {
				double d = prev_max - prev_min;
				prev_max += d/4;
				if (prev_min-d/4 >= 0) prev_min = prev_min-d/4;
			}
		}
		CatClassifHistCanvas::InitUniformData(preview_data, prev_min, prev_max);
	}
	hist_canvas->ChangeAll(&preview_data,
						   &cc_data.breaks, &cc_data.colors);
	Refresh();
	LOG_MSG("Exiting CatClassifPanel::OnPreviewVarChoice");
}

void CatClassifPanel::OnPreviewVarTmChoice(wxCommandEvent& ev)
{
	LOG_MSG("Entering CatClassifPanel::OnPreviewVarTmChoice");
	if (preview_var_choice->GetSelection() == 0) return;
	if (GetPreviewDbFldNm() == "") {
		SetSyncVars(true);
		InitFromCCData();
		return;
	}
	int col, tm;
	table_int->DbColNmToColAndTm(GetPreviewDbFldNm(), col, tm);
	std::vector<double> dd;
	table_int->GetColData(col, tm, dd);
	for (size_t i=0, sz=dd.size(); i<sz; ++i) {
		preview_data[i].first = dd[i];
		preview_data[i].second = i;
	}
	hist_canvas->ChangeAll(&preview_data,
						   &cc_data.breaks, &cc_data.colors);
	Refresh();	
	LOG_MSG("Exiting CatClassifPanel::OnPreviewVarTmChoice");
}

void CatClassifPanel::OnSyncVarsChk(wxCommandEvent& ev)
{
	LOG_MSG("In CatClassifPanel::OnSyncVarsChk");
	// IsSyncVars() reflects the new value of the checkbox.  This
	// callback is called after the value has changed.
	preview_var_choice->Enable(!IsSyncVars());
	preview_var_tm_choice->Enable(!IsSyncVars());
	if (IsSyncVars()) InitFromCCData();
	LOG_MSG("Exiting CatClassifPanel::OnSyncVarsChk");
}

void CatClassifPanel::OnUnifDistMinEnter(wxCommandEvent& event)
{
	LOG_MSG("In CatClassifPanel::OnUnifDistMinEnter");
	if (!all_init || !IsUnifDistMode()) return;
	// When in uniform dist mode, there is no variable associated
	// with the breaks.  Therefore the data must be resampled whenever
	// the uniform dist min/max value is changed and the existing
	// breaks might need to be modified as well.
	wxString s(unif_dist_min_txt->GetValue());
	double val;
	if (s.ToDouble(&val)) {
		if (val != cc_data.uniform_dist_min) {
			// Proceed with change
			cc_data.uniform_dist_min = val;
			if (cc_data.uniform_dist_min > cc_data.uniform_dist_max) {
				double t = cc_data.uniform_dist_min;
				cc_data.uniform_dist_min = cc_data.uniform_dist_max;
				cc_data.uniform_dist_max = t;
			}
			// Adjust break values to fit min and max
			for (int i=0, sz=cc_data.breaks.size(); i<sz; ++i) {
				if (cc_data.breaks[i] < cc_data.uniform_dist_min) {
					cc_data.breaks[i] = cc_data.uniform_dist_min;
				}
				if (cc_data.breaks[i] > cc_data.uniform_dist_max) {
					cc_data.breaks[i] = cc_data.uniform_dist_max;
				}
			}
			InitFromCCData();
			UpdateCCState();
		}
	} else {
		// restore to original value
		wxString s;
		s << cc_data.uniform_dist_min;
		unif_dist_min_txt->ChangeValue(s);
	}
}

void CatClassifPanel::OnUnifDistMinKillFocus(wxFocusEvent& event)
{
	LOG_MSG("In CatClassifPanel::OnUnifDistMinKillFocus");
	wxCommandEvent ev;
	OnUnifDistMinEnter(ev);
	event.Skip();
}

void CatClassifPanel::OnUnifDistMaxEnter(wxCommandEvent& event)
{
	LOG_MSG("In CatClassifPanel::OnUnifDistMaxEnter");
	if (!all_init || !IsUnifDistMode()) return;
	// When in uniform dist mode, there is no variable associated
	// with the breaks.  Therefore the data must be resampled whenever
	// the uniform dist min/max value is changed and the existing
	// breaks might need to be modified as well.
	wxString s(unif_dist_max_txt->GetValue());
	double val;
	if (s.ToDouble(&val)) {
		if (val != cc_data.uniform_dist_max) {
			// Proceed with change
			cc_data.uniform_dist_max = val;
			if (cc_data.uniform_dist_min > cc_data.uniform_dist_max) {
				double t = cc_data.uniform_dist_min;
				cc_data.uniform_dist_min = cc_data.uniform_dist_max;
				cc_data.uniform_dist_max = t;
			}
			// Adjust break values to fit min and max
			for (int i=0, sz=cc_data.breaks.size(); i<sz; ++i) {
				if (cc_data.breaks[i] < cc_data.uniform_dist_min) {
					cc_data.breaks[i] = cc_data.uniform_dist_min;
				}
				if (cc_data.breaks[i] > cc_data.uniform_dist_max) {
					cc_data.breaks[i] = cc_data.uniform_dist_max;
				}
			}
			InitFromCCData();
			UpdateCCState();
		}
	} else {
		// restore to original value
		wxString s;
		s << cc_data.uniform_dist_max;
		unif_dist_max_txt->ChangeValue(s);
	}
}

void CatClassifPanel::OnUnifDistMaxKillFocus(wxFocusEvent& event)
{
	LOG_MSG("In CatClassifPanel::OnUnifDistMaxKillFocus");
	wxCommandEvent ev;
	OnUnifDistMaxEnter(ev);
	event.Skip();
}

void CatClassifPanel::OnAutomaticLabelsCb(wxCommandEvent& event)
{
	LOG_MSG("In CatClassifPanel::OnAutomaticLabelsCb");
	if (event.IsChecked()) {
		cc_data.automatic_labels = true;
		LOG_MSG("turning automatic labels on");
		SetBrkTxtFromVec(cc_data.breaks);
		UpdateCCState();
		Refresh();
	} else {
		cc_data.automatic_labels = false;
		LOG_MSG("turning automatic labels off");
	}
	LOG_MSG("automatic_labels: " + GenUtils::BoolToStr(event.IsChecked()));
}

void CatClassifPanel::OnBrkRad(wxCommandEvent& event)
{
	if (!all_init) return;
	wxRadioButton* obj = (wxRadioButton*) event.GetEventObject();
	int obj_id = -1;
	for (int i=0, iend=brk_rad.size(); i<iend && obj_id==-1; i++) {
		if (obj == brk_rad[i]) obj_id = i;
	}
	LOG(obj_id);
	if (obj_id < 0) return;
	SetSliderFromBreak(obj_id);
}

void CatClassifPanel::OnBrkTxtEnter(wxCommandEvent& event)
{
	if (!all_init) return;
	wxTextCtrl* obj = (wxTextCtrl*) event.GetEventObject();
	int obj_id = -1;
	for (int i=0, iend=brk_txt.size(); i<iend && obj_id==-1; i++) {
		if (obj == brk_txt[i]) obj_id = i;
	}
	LOG_MSG("In CatClassifPanel::OnBrkTxtEnter");
	LOG(obj_id);
	if (obj_id < 0) return;
	wxString s(brk_txt[obj_id]->GetValue());
	double val;
	if (s.ToDouble(&val)) {
		if (val != cc_data.breaks[obj_id]) {
			SetBreakValsTypeChoice(CatClassification::custom_break_vals);
			int nbrk = CatClassification::ChangeBreakValue(obj_id, val,
														   cc_data);
			SetBrkTxtFromVec(cc_data.breaks);
			SetActiveBrkRadio(nbrk);
			SetUnifDistMinMaxTxt(cc_data.uniform_dist_min,
								 cc_data.uniform_dist_max);
			UpdateCCState();
			hist_canvas->ChangeAll(&preview_data, &cc_data.breaks,
								   &cc_data.colors);
			SetSliderFromBreak(nbrk);
		}
	} else {
		wxString o;
		o << cc_data.breaks[obj_id];
		brk_txt[obj_id]->ChangeValue(o);
	}
}

void CatClassifPanel::OnBrkSlider(wxCommandEvent& event)
{
	if (!all_init) return;
	if (!brk_slider->IsEnabled()) return;
	if (last_brk_slider_pos == brk_slider->GetValue()) return;
	last_brk_slider_pos = brk_slider->GetValue();
	//LOG_MSG("In CatClassifPanel::OnBrkSlider");
	int brk = GetActiveBrkRadio();
	if (brk < 0 || brk > GetNumCats()-2) return;
	double sr = ((double) (brk_slider->GetMax()-brk_slider->GetMin()));
	if (sr == 0) return;
	double sd = ((double) (brk_slider->GetValue()-brk_slider->GetMin()));
	double r = GetBrkSliderMax()-GetBrkSliderMin();
	double v = GetBrkSliderMin() + (sd/sr)*r;
	LOG(v);
	cc_data.breaks[brk] = v;
	brk_txt[brk]->ChangeValue(GenUtils::DblToStr(v));
	int nbrk = CatClassification::ChangeBreakValue(brk, v, cc_data);
	// refresh just the radio buttons and break txt
	SetBreakValsTypeChoice(CatClassification::custom_break_vals);
	SetBrkTxtFromVec(cc_data.breaks);
	SetActiveBrkRadio(nbrk);
	UpdateCCState();
	hist_canvas->ChangeAll(&preview_data, &cc_data.breaks, &cc_data.colors);
}

void CatClassifPanel::OnScrollThumbRelease(wxScrollEvent& event)
{
	//LOG_MSG("In CatClassifPanel::OnScrollThumbRelease");
	SetSliderFromBreak(GetActiveBrkRadio());
}

void CatClassifPanel::OnKillFocusEvent(wxFocusEvent& event)
{
	LOG_MSG("In CatClassifPanel::OnKillFocusEvent");
	wxWindow* w = (wxWindow*) (event.GetEventObject());
	if (wxTextCtrl* tc = dynamic_cast<wxTextCtrl*>(w)) {
		LOG(tc->GetValue());
		int obj_id = -1;
		for (size_t i=0; i<cc_data.breaks.size() && obj_id<0; i++) {
			if (tc == brk_txt[i]) obj_id = i;
		}
		if (obj_id != -1) {
			LOG_MSG(wxString::Format("Focus left brk_text[%d]", obj_id));
			wxString s(brk_txt[obj_id]->GetValue());
			double val;
			if (s.ToDouble(&val)) {
				if (val != cc_data.breaks[obj_id]) {
					SetBreakValsTypeChoice(
									CatClassification::custom_break_vals);
					int nbrk = CatClassification::ChangeBreakValue(obj_id, val,
																   cc_data);
					SetBrkTxtFromVec(cc_data.breaks);
					SetActiveBrkRadio(nbrk);
					hist_canvas->ChangeAll(&preview_data, &cc_data.breaks,
										   &cc_data.colors);
					min_lbl->SetLabelText(
									GenUtils::DblToStr(GetBrkSliderMin()));
					max_lbl->SetLabelText(
									GenUtils::DblToStr(GetBrkSliderMin()));
					SetSliderFromBreak(nbrk);
					UpdateCCState();
					Refresh();
				}
			} else {
				wxString o;
				o << cc_data.breaks[obj_id];
				brk_txt[obj_id]->ChangeValue(o);
			}
		}
	}
	event.Skip();
}

void CatClassifPanel::OnCategoryColorButton(wxMouseEvent& event)
{
	if (!all_init) return;
	int obj_id = -1;
	
	wxStaticBitmap* obj = (wxStaticBitmap*) event.GetEventObject();
	for (int i=0, iend=cat_color_button.size(); i<iend && obj_id==-1; i++) {
		if (obj == cat_color_button[i]) obj_id = i;
	}
	
	//int pos_x; int pos_y;
	//event.GetPosition(&pos_x, &pos_y);
	//for (int i=0, iend=cat_color_button.size(); i<iend && obj_id==-1; i++) {
	//	int x, y, w, h;
	//	cat_color_button[i]->GetPosition(&x, &y);
	//	cat_color_button[i]->GetSize(&w, &h);
	//	if ((pos_x >= x && pos_x <= x+w) &&
	//		(pos_y >= y && pos_y <= y+h)) obj_id = i;
	//}
	LOG(obj_id);
	if (obj_id < 0) return;
	
	wxColour col = cc_data.colors[obj_id];
	wxColourData clr_data;
	clr_data.SetColour(col);
	clr_data.SetChooseFull(true);
	int ki;
	for (ki = 0; ki < 16; ki++) {
		wxColour colour(ki * 16, ki * 16, ki * 16);
		clr_data.SetCustomColour(ki, colour);
	}
	
	wxColourDialog dialog(this, &clr_data);
	dialog.SetTitle("Choose Cateogry Color");
	if (dialog.ShowModal() != wxID_OK) return;
	wxColourData retData = dialog.GetColourData();
	if (cc_data.colors[obj_id] == retData.GetColour()) return;
	SetColorSchemeChoice(CatClassification::custom_color_scheme);
	cc_data.color_scheme = CatClassification::custom_color_scheme;
	cc_data.colors[obj_id] = retData.GetColour();
	cat_color_button[obj_id]->SetBackgroundColour(retData.GetColour());
	UpdateCCState();
	hist_canvas->ChangeAll(&preview_data, &cc_data.breaks, &cc_data.colors);
	Refresh();
}

void CatClassifPanel::OnCategoryTitleText(wxCommandEvent& event)
{
	if (!all_init) return;
	wxTextCtrl* obj = (wxTextCtrl*) event.GetEventObject();
	int obj_id = -1;
	for (int i=0, iend=cat_title_txt.size(); i<iend && obj_id==-1; i++) {
		if (obj == cat_title_txt[i]) obj_id = i;
	}
	LOG(obj_id);
	if (obj_id < 0) return;
	cc_data.names[obj_id] = cat_title_txt[obj_id]->GetValue();
	UpdateCCState();
}

void CatClassifPanel::OnButtonChangeTitle(wxCommandEvent& event)
{
	if (!all_init) return;
	wxString msg;
	wxString cur_title = cur_cats_choice->GetStringSelection();
	wxString new_title = cur_title;
	msg << "Change title \"" << cur_title << "\" to";
	bool retry = true;
	while (retry) {
		wxTextEntryDialog dlg(this, msg, "Change Categories Title");
		dlg.SetValue(new_title);
		if (dlg.ShowModal() == wxID_OK) {
			new_title = dlg.GetValue();
			new_title.Trim(false);
			new_title.Trim(true);
			if (new_title.IsEmpty() || new_title == cur_title) {
				retry = false;
			} else if (IsDuplicateTitle(new_title)) {
				wxString es;
				es << "Categories title \"" << new_title << "\" already ";
				es << "exists. Please choose a different title.";
				wxMessageDialog ed(NULL, es, "Error", wxOK | wxICON_ERROR);
				ed.ShowModal();
			} else {
				int sel = cur_cats_choice->GetSelection();
				cur_cats_choice->SetString(sel, new_title);
				cc_data.title = new_title;
				UpdateCCState();
				retry = false;
			}
		} else {
			retry = false;
		}
	}
}

void CatClassifPanel::OnButtonNew(wxCommandEvent& event)
{
	if (!all_init) return;
	wxString msg;
	msg << "New Custom Categories Title:";
	wxString new_title = GetDefaultTitle();
	bool retry = true;
	while (retry) {
		wxTextEntryDialog dlg(this, msg, "New Categories Title");
		dlg.SetValue(new_title);
		if (dlg.ShowModal() == wxID_OK) {
			new_title = dlg.GetValue();
			new_title.Trim(false);
			new_title.Trim(true);
			if (new_title.IsEmpty()) {
				retry = false;
			} else if (IsDuplicateTitle(new_title)) {
				wxString es;
				es << "Categories title \"" << new_title << "\" already ";
				es << "exists. Please choose a different title.";
				wxMessageDialog ed(NULL, es, "Error", wxOK | wxICON_ERROR);
				ed.ShowModal();
			} else {
				cc_data.title = new_title;
				cc_data.assoc_db_fld_name = GetAssocDbFldNm();
				cc_state = cat_classif_manager->CreateNewClassifState(cc_data);
				cur_cats_choice->Append(new_title);
				cur_cats_choice->SetSelection(cur_cats_choice->GetCount()-1);
				SetSyncVars(true);
				InitFromCCData();
				EnableControls(true);
				retry = false;
			}
		} else {
			retry = false;
		}
	}
}

void CatClassifPanel::OnButtonDelete(wxCommandEvent& event)
{
	if (!all_init) return;
	wxString custom_cat_title = cur_cats_choice->GetStringSelection();
	if (IsOkToDelete(custom_cat_title)) {
		cat_classif_manager->RemoveClassifState(cc_state);
		cc_state = 0;
		cur_cats_choice->Delete(cur_cats_choice->GetSelection());
		SetSyncVars(true);
		if (cur_cats_choice->GetCount() > 0) {
			cur_cats_choice->SetSelection(0);
			cc_state = cat_classif_manager->
			FindClassifState(cur_cats_choice->GetStringSelection());
			cc_data = cc_state->GetCatClassif();
			InitFromCCData();
		} else {
			ResetValuesToDefault();
			CatClassifHistCanvas::InitUniformData(data,
												  default_min, default_max);
			CatClassifHistCanvas::InitUniformData(preview_data,
												  default_min, default_max);
			hist_canvas->ChangeAll(&preview_data,
								   &cc_data.breaks, &cc_data.colors);
			EnableControls(false);
		}
	} else {
		wxString es;
		es << "Categories \"" << custom_cat_title << "\" is currently ";
		es << "in use by another view. Please close or change all views ";
		es << "using this custom categories before deleting.";
		wxMessageDialog ed(NULL, es, "Error", wxOK | wxICON_ERROR);
		ed.ShowModal();
	}
}

void CatClassifPanel::OnButtonClose(wxCommandEvent& event)
{
	template_frame->Close();
}

/** Reset cc_data and all controls according to default_intervals.
 Assumes controls have already been created.
 To be reset:
 - Num Cats to default
 - uniform dist mode
 - preview_var selection to uniform dist
 - preview_var time to first and disable
 - unif dist min to 0, unif dist max to 1
 - automatic_labels to true
 - data to uniform dist
 - assoc_db_fld_name to empty
 - cat classif type to Quantile
 - Break values to uniform dist
 - cat names to default
 - slider min/max to 0/1
 - slider radio to first
 - slider value to match first break 
 - cat colors / color scheme to diverging
 
 Not to be reset:
 - title
 */
void CatClassifPanel::ResetValuesToDefault()
{
	SetNumCats(default_intervals);
	CatClassification::ChangeNumCats(default_intervals, cc_data);
	cc_data.num_cats = default_intervals;
	
	cc_data.automatic_labels = true;
	SetAutomaticLabels(true);
	
	unif_dist_mode = true;
	assoc_var_choice->SetSelection(0);
	assoc_var_tm_choice->SetSelection(0);
	assoc_var_tm_choice->Enable(false);

	preview_var_choice->SetSelection(0);
	preview_var_tm_choice->SetSelection(0);
	preview_var_tm_choice->Enable(false);
	
	sync_vars_chk->SetValue(true);
	sync_vars_chk->Enable(false);
	
	cc_data.uniform_dist_min = default_min;
	cc_data.uniform_dist_max = default_max;
	ShowUnifDistMinMax(true);
	{
		wxString s;
		s << default_min;
		unif_dist_min_txt->ChangeValue(s);
	}
	{
		wxString s;
		s << default_max;
		unif_dist_max_txt->ChangeValue(s);
	}
	
	CatClassifHistCanvas::InitUniformData(data, default_min,
										  default_max);
	CatClassifHistCanvas::InitUniformData(preview_data, default_min,
										  default_max);
	
	cc_data.assoc_db_fld_name = "";
	
	cc_data.names.clear();
	cc_data.break_vals_type = CatClassification::quantile_break_vals;
	CatClassification::SetBreakPoints(cc_data.breaks, cc_data.names, data,
									  CatClassification::quantile,
									  default_intervals);
	
	for (int i=0; i<max_intervals; ++i) {
		cat_title_txt[i]->ChangeValue("");
		if (i < default_intervals) {
			cat_title_txt[i]->ChangeValue(cc_data.names[i]);
		}
	}
	for (int i=0; i<max_intervals-1; ++i) {
		wxString s;
		if (i < default_intervals-1) {
			s << cc_data.breaks[i];
		} else {
			s << default_max;
		}
		brk_txt[i]->ChangeValue(s); // no events emitted
	}
	brk_rad[0]->SetValue(true); // no events emitted and all others set to false
	SetSliderFromBreak(0);
	
	SetColorSchemeChoice(CatClassification::diverging_color_scheme);
	CatClassification::PickColorSet(cc_data.colors,
									CatClassification::diverging_color_scheme,
									default_intervals, false);
	for (int i=0; i<default_intervals; i++) {
		cat_color_button[i]->SetBackgroundColour(cc_data.colors[i]);
	}
	
	Refresh();
}

void CatClassifPanel::EnableControls(bool enable)
{	
	cur_cats_choice->Enable(enable);
	breaks_choice->Enable(enable);
	change_title_button->Enable(enable);
	delete_button->Enable(enable);
	num_cats_choice->Enable(enable);
	auto_labels_cb->Enable(enable);
	color_scheme->Enable(enable);
	assoc_var_choice->Enable(enable);
	assoc_var_tm_choice->Enable(enable);
	preview_var_choice->Enable(enable && !IsSyncVars());
	preview_var_tm_choice->Enable(enable && !IsSyncVars());
	sync_vars_chk->Enable(enable);
	unif_dist_min_txt->Enable(enable);
	unif_dist_max_txt->Enable(enable);
	brk_slider->Enable(enable);
	for (size_t i=0; i<cat_color_button.size(); i++) {
		cat_color_button[i]->Enable(enable);
		cat_title_txt[i]->Enable(enable);
	}
	for (size_t i=0; i<brk_rad.size(); i++) {
		brk_rad[i]->Enable(enable);
		brk_txt[i]->Enable(enable);
	}
}

/** Initialize all controls and CatClassifHistogram according to current
 cc_data and associated variable.  Additionally, if IsSyncVars true,
 then preview_data and data vectors will be synchronized as well as
 current Preview Variable selections.  If IsSyncVars false, breaks will
 be recalculated, but current Preview Variable data will be used for the
 histogram.
 
 Controls:
 Initialized here from cc_data:
 - associated variable and time choices
 - preview variable and time choices
 - breaks selection from cc_data.break_vals_type
 - color scheme
 - # categories / GetNumCats() from cc_data.breaks.size()+1
 - automatic labels checkbox
 - category colors
 - category names
 - break values
 - particular col/field and time choice
 - fill data depending on col name or uni dist
 - uni dist min/max txt depending on uni dist mode
 - slider min/max and slider position
 */
void CatClassifPanel::InitFromCCData()
{
	// Verify that cc data is self-consistent and correct if not.  This
	// will result in all breaks, colors and names being initialized.
	CatClassification::CorrectCatClassifFromTable(cc_data, table_int);
	
	SetColorSchemeChoice(cc_data.color_scheme);
	SetUnifDistMode(cc_data.assoc_db_fld_name.IsEmpty());
	SetNumCats(cc_data.num_cats);
	for (int i=0; i<cc_data.num_cats; i++) {
		cat_color_button[i]->SetBackgroundColour(cc_data.colors[i]);
		cat_title_txt[i]->ChangeValue(cc_data.names[i]);
	}
	SetBrkTxtFromVec(cc_data.breaks);
	SetActiveBrkRadio(0);
	ShowNumCategories(cc_data.num_cats);
	SetAutomaticLabels(cc_data.automatic_labels);
	if (IsUnifDistMode()) {
		assoc_var_choice->SetSelection(0); // uniform distribution
		assoc_var_tm_choice->Enable(false);
		if (IsSyncVars()) {
			preview_var_choice->SetSelection(0);
			preview_var_tm_choice->Enable(false);
		}
		CatClassifHistCanvas::InitUniformData(preview_data,
											  cc_data.uniform_dist_min,
											  cc_data.uniform_dist_max);
	} else {
		int col, tm;
		table_int->DbColNmToColAndTm(cc_data.assoc_db_fld_name, col, tm);
		int sel = assoc_var_choice->FindString(table_int->GetColName(col));
		assoc_var_choice->SetSelection(sel);
		if (table_int->IsColTimeVariant(col)) {
			assoc_var_tm_choice->Enable(true);
			assoc_var_tm_choice->SetSelection(tm);
		} else {
			assoc_var_tm_choice->Enable(false);
		}
		if (IsSyncVars()) {
			preview_var_choice->SetSelection(sel);
			if (table_int->IsColTimeVariant(col)) {
				preview_var_tm_choice->Enable(true);
				preview_var_tm_choice->SetSelection(tm);
			} else {
				preview_var_tm_choice->Enable(false);
			}
		}
		std::vector<double> dd;
		table_int->GetColData(col, tm, dd);
		for (int i=0; i<num_obs; i++) {
			data[i].first = dd[i];
			data[i].second = i;
		}
		if (IsSyncVars()) {
			for (int i=0; i<num_obs; i++) {
				preview_data[i].first = dd[i];
				preview_data[i].second = i;
			}
		}
		std::sort(data.begin(), data.end(), Gda::dbl_int_pair_cmp_less);
		if (IsSyncVars()) {
			std::sort(preview_data.begin(),
					  preview_data.end(), Gda::dbl_int_pair_cmp_less);
		}
	}
	
	SetBreakValsTypeChoice(cc_data.break_vals_type);
	
	SetUnifDistMinMaxTxt(cc_data.uniform_dist_min, cc_data.uniform_dist_max);
	ShowUnifDistMinMax(IsUnifDistMode());
	
	SetSliderFromBreak(0);

	hist_canvas->ChangeAll(&preview_data, &cc_data.breaks, &cc_data.colors);
	Refresh();
}

/**
 Assumptions: all widgets are initialized
 Purpose: Initialize assoc_var_choice and assoc_var_tm_choice
   widgets.  If any current selections, then these are remembered
   and reselected. Note: InitAssocVarChoices is based on methods
   in DataMovieDlg class.
 */
void CatClassifPanel::InitAssocVarChoices()
{
	LOG_MSG("Entering CatClassifPanel::InitAssocVarChoices");
	if (!all_init) return;
	wxString cur_fc_str = assoc_var_choice->GetStringSelection();
	int cur_fc_tm_id = assoc_var_tm_choice->GetSelection();
	assoc_var_choice->Clear();
	assoc_var_tm_choice->Clear();
	std::vector<wxString> times;
	project->GetTableInt()->GetTimeStrings(times);
	for (size_t i=0; i<times.size(); i++) {
		assoc_var_tm_choice->Append(times[i]);
	}
	if (cur_fc_tm_id != wxNOT_FOUND) {
		assoc_var_tm_choice->SetSelection(cur_fc_tm_id);
	}
	std::vector<wxString> names;
	table_int->FillNumericNameList(names);
	assoc_var_choice->Append(unif_dist_txt);
	for (size_t i=0; i<names.size(); i++) {
		assoc_var_choice->Append(names[i]);
	}
	assoc_var_choice->SetSelection(assoc_var_choice->
										FindString(cur_fc_str));
	assoc_var_tm_choice->Enable(table_int->IsColTimeVariant(cur_fc_str));
	if (table_int->IsColTimeVariant(cur_fc_str) &&
		assoc_var_tm_choice->GetSelection() == wxNOT_FOUND) {
		assoc_var_tm_choice->SetSelection(0);
	}
	
	if (assoc_var_choice->FindString(cur_fc_str) == wxNOT_FOUND) {
		// default to uniform distribution
		assoc_var_choice->SetSelection(0);
	}
	LOG(table_int->IsColTimeVariant(cur_fc_str));
	LOG_MSG("Exiting CatClassifPanel::InitAssocVarChoices");
}


/**
 Assumptions: all widgets are initialized
 Purpose: Initialize preview_var_choice and preview_var_tm_choice
   widgets.  If any current selections, then these are remembered
   and reselected. Note: InitPreviewVarChoices is based on methods
   in DataMovieDlg class.
 */
void CatClassifPanel::InitPreviewVarChoices()
{
	LOG_MSG("Entering CatClassifPanel::InitPreviewVarChoices");
	if (!all_init) return;
	wxString cur_fc_str = preview_var_choice->GetStringSelection();
	int cur_fc_tm_id = preview_var_tm_choice->GetSelection();
	preview_var_choice->Clear();
	preview_var_tm_choice->Clear();
	std::vector<wxString> times;
	project->GetTableInt()->GetTimeStrings(times);
	for (size_t i=0; i<times.size(); i++) {
		preview_var_tm_choice->Append(times[i]);
	}
	if (cur_fc_tm_id != wxNOT_FOUND) {
		preview_var_tm_choice->SetSelection(cur_fc_tm_id);
	}
	std::vector<wxString> names;
	table_int->FillNumericNameList(names);
	preview_var_choice->Append(unif_dist_txt);
	for (size_t i=0; i<names.size(); i++) {
		preview_var_choice->Append(names[i]);
	}
	preview_var_choice->SetSelection(preview_var_choice->
										FindString(cur_fc_str));
	preview_var_tm_choice->Enable(table_int->IsColTimeVariant(cur_fc_str));
	if (table_int->IsColTimeVariant(cur_fc_str) &&
		preview_var_tm_choice->GetSelection() == wxNOT_FOUND) {
		preview_var_tm_choice->SetSelection(0);
	}
	
	if (preview_var_choice->FindString(cur_fc_str) == wxNOT_FOUND) {
		// default to first item
		preview_var_choice->SetSelection(0);
	}
	LOG(table_int->IsColTimeVariant(cur_fc_str));
	LOG_MSG("Exiting CatClassifPanel::InitPreivewVarChoices");
}


/** Populate the Custom Categories Choices from Classification Manager.
 Will attempt to keep the current selection if it still exists.  This
 is only called in the constructor currently. */
void CatClassifPanel::InitCurCatsChoices()
{
	LOG_MSG("Entering CatClassifPanel::InitCutCatsChoices");
	if (!all_init) return;
	wxString cur_str = cur_cats_choice->GetStringSelection();
	cur_cats_choice->Clear();
	std::vector<wxString> titles;
	cat_classif_manager->GetTitles(titles);
	for (int i=0, ie=titles.size(); i<ie; i++) {
		cur_cats_choice->Append(titles[i]);
	}
	if (cur_cats_choice->GetCount() == 0) return;
	int sel = cur_cats_choice->FindString(cur_str);
	if (sel != wxNOT_FOUND) {
		cur_cats_choice->SetSelection(sel);
	} else {
		cur_cats_choice->SetSelection(0);
	}
	LOG_MSG("Exiting CatClassifPanel::InitCutCatsChoices");
}

/** Gets number of categories from num_cats_choice */
int CatClassifPanel::GetNumCats()
{
	if (!all_init) return default_intervals;
	if (num_cats_choice->GetSelection() < 0) return default_intervals;
	return num_cats_choice->GetSelection()+1;
}

/** Sets num_cats_choice widget */
void CatClassifPanel::SetNumCats(int num_cats)
{
	if (!all_init || num_cats < 1 || num_cats > max_intervals) return;
	num_cats_choice->SetSelection(num_cats-1);
}

/** Show/hide widgets for breaks radio/text/label, color button and
 cat titles */
void CatClassifPanel::ShowNumCategories(int num_cats)
{
	if (num_cats < 1 || num_cats > max_intervals) return;
	for (int i=0; i<max_intervals; i++) {
		bool show = i<num_cats;
		cat_color_button[i]->Show(show);
		cat_title_txt[i]->Show(show);
	}
	for (int i=0; i<max_intervals-1; i++) {
		bool show = i<num_cats-1;
		brk_rad[i]->Show(show);
		brk_lbl[i]->Show(show);
		brk_txt[i]->Show(show);
	}
}

bool CatClassifPanel::IsAutomaticLabels()
{
	if (!all_init) return true;
	return auto_labels_cb->GetValue();
}

void CatClassifPanel::SetAutomaticLabels(bool auto_labels)
{
	if (!all_init) return;
	auto_labels_cb->SetValue(auto_labels);
}

/** Get color scheme from color_scheme choice widget */
CatClassification::ColorScheme CatClassifPanel::GetColorSchemeChoice()
{
	if (!all_init) return CatClassification::diverging_color_scheme;
	if (color_scheme->GetSelection() == 0)
		return CatClassification::sequential_color_scheme;
	if (color_scheme->GetSelection() == 1)
		return CatClassification::diverging_color_scheme;
	if (color_scheme->GetSelection() == 2)
		return CatClassification::qualitative_color_scheme;
	return CatClassification::custom_color_scheme;
}

/** Set color_scheme choice widget */
void CatClassifPanel::SetColorSchemeChoice(CatClassification::ColorScheme cs)
{
	if (!all_init) return;
	if (cs == CatClassification::sequential_color_scheme) {
		color_scheme->SetSelection(0);
	} else if (cs == CatClassification::diverging_color_scheme) {
		color_scheme->SetSelection(1);
	} else if (cs == CatClassification::qualitative_color_scheme) {
		color_scheme->SetSelection(2);
	} else { // custom
		color_scheme->SetSelection(3);
	}
}

/** Get classification type from breaks_choice widget */
CatClassification::BreakValsType CatClassifPanel::GetBreakValsTypeChoice()
{
	int brs = breaks_choice->GetSelection();
	if (brs == 0) return CatClassification::no_theme_break_vals;
	if (brs == 1) return CatClassification::quantile_break_vals;
	if (brs == 2) return CatClassification::unique_values_break_vals;
	if (brs == 3) return CatClassification::natural_breaks_break_vals;
	if (brs == 4) return CatClassification::equal_intervals_break_vals;
	if (brs == 5) return CatClassification::custom_break_vals;
	// quantile by default
	return CatClassification::quantile_break_vals;
}

/** Set breaks_choice widget classification type */
void CatClassifPanel::SetBreakValsTypeChoice(
									CatClassification::BreakValsType ct)
{
	int brs = 1; // quantile by default
	if (ct == CatClassification::no_theme_break_vals) brs = 0;
	if (ct == CatClassification::quantile_break_vals) brs = 1;
	if (ct == CatClassification::unique_values_break_vals) brs = 2;
	if (ct == CatClassification::natural_breaks_break_vals) brs = 3;
	if (ct == CatClassification::equal_intervals_break_vals) brs = 4;
	if (ct == CatClassification::custom_break_vals) brs = 5;
	
	if (ct == CatClassification::percentile_break_vals) brs = 5;
	if (ct == CatClassification::hinge_15_break_vals) brs = 5;
	if (ct == CatClassification::hinge_30_break_vals) brs = 5;
	if (ct == CatClassification::stddev_break_vals) brs = 5;
	breaks_choice->SetSelection(brs);	
}

/** Returns current data base field name based on values in
 assoc_var_choice and assoc_var_tm_choice.  If placeholder,
 uniform distribution or simple not found in TableInterface
 than empty string is returned. */
wxString CatClassifPanel::GetAssocDbFldNm()
{
	if (!assoc_var_choice || !assoc_var_tm_choice) return "";
	if (assoc_var_choice->GetSelection() == 0) return "";
	if (!assoc_var_choice->IsEnabled() ||
		!assoc_var_choice->IsShown()) return "";
	int tm = assoc_var_tm_choice->GetSelection();
	if (tm < 0) tm = 0;
	if (!assoc_var_tm_choice->IsEnabled() ||
		!assoc_var_tm_choice->IsShown()) tm = 0;
	wxString grp_nm = assoc_var_choice->GetStringSelection();
	int col = table_int->FindColId(grp_nm);
	return table_int->GetColName(col, tm);
}

wxString CatClassifPanel::GetAssocVarChoice()
{
	if (!all_init) return "";
	return assoc_var_choice->GetStringSelection();
}

wxString CatClassifPanel::GetAssocVarTmChoice()
{
	if (!all_init) return "";
	return assoc_var_tm_choice->GetStringSelection();
}

int CatClassifPanel::GetAssocVarTmAsInt()
{
	if (!all_init) return 0;
	return table_int->GetTimeInt(GetAssocVarTmChoice());
}

/** Returns current data base field name based on values in
 preview_var_choice and preview_var_tm_choice.  If placeholder,
 uniform distribution or simple not found in TableInterface
 than empty string is returned. */
wxString CatClassifPanel::GetPreviewDbFldNm()
{
	if (!preview_var_choice || !preview_var_tm_choice) return "";
	if (preview_var_choice->GetSelection() == 0) return "";
	int tm = preview_var_tm_choice->GetSelection();
	if (tm < 0) tm = 0;
	if (!preview_var_tm_choice->IsShown()) tm = 0;
	wxString grp_nm = preview_var_choice->GetStringSelection();
	int col = table_int->FindColId(grp_nm);
	return table_int->GetColName(col, tm);
}

wxString CatClassifPanel::GetPreviewVarChoice()
{
	if (!all_init) return "";
	return preview_var_choice->GetStringSelection();
}

wxString CatClassifPanel::GetPreviewVarTmChoice()
{
	if (!all_init) return "";
	return preview_var_tm_choice->GetStringSelection();
}

int CatClassifPanel::GetPreviewVarTmAsInt()
{
	if (!all_init) return 0;
	return table_int->GetTimeInt(GetPreviewVarTmChoice());
}

bool CatClassifPanel::IsSyncVars()
{
	return sync_vars_chk->GetValue();
}

void CatClassifPanel::SetSyncVars(bool sync_assoc_and_prev_vars)
{
	sync_vars_chk->SetValue(sync_assoc_and_prev_vars);
}

void CatClassifPanel::ShowUnifDistMinMax(bool show)
{
	unif_dist_min_lbl->Show(show);
	unif_dist_min_txt->Show(show);
	unif_dist_max_lbl->Show(show);
	unif_dist_max_txt->Show(show);
}

void CatClassifPanel::SetUnifDistMinMaxTxt(double min, double max)
{
	wxString min_txt;
	min_txt << min;
	unif_dist_min_txt->ChangeValue(min_txt);
	wxString max_txt;
	max_txt << max;
	unif_dist_max_txt->ChangeValue(max_txt);
}

int CatClassifPanel::GetActiveBrkRadio()
{
	if (!all_init) return -1;
	for (size_t i=0; i<brk_rad.size(); i++) {
		if (brk_rad[i]->GetValue() != 0) return i;
	}
	return -1;
}

void CatClassifPanel::SetActiveBrkRadio(int i)
{
	if (!all_init) return;
	if (i < 0 || i > max_intervals) return;
	brk_rad[i]->SetValue(1);
}

void CatClassifPanel::SetBrkTxtFromVec(const std::vector<double>& brks)
{
	for (int i=0, sz=brks.size(); i<sz && i<max_intervals; ++i) {
		brk_txt[i]->ChangeValue(GenUtils::DblToStr(brks[i]));
	}
	if (IsAutomaticLabels()) {
		std::vector<wxString> new_labels;
		CatClassification::CatLabelsFromBreaks(brks, new_labels);
		int sz = new_labels.size();
		cc_data.names.resize(sz);
		for (int i=0; i<sz; ++i) {
			cc_data.names[i] = new_labels[i];
			cat_title_txt[i]->ChangeValue(new_labels[i]);
		}
	}
}

/** Current minimum valid value for all Breaks Slider controls.  We
 ensure that max-min in never zero.  Also, min cannot be below zero
 unless actual data points already go below zero. */
double CatClassifPanel::GetBrkSliderMin()
{
	double min = cc_data.uniform_dist_min;
	double max = cc_data.uniform_dist_max;
	double range = max-min;
	double sl_min;
	double sl_max;
	if (min == 0 && max == 0) {
		sl_min = 0;
		sl_max = 1;
	} else if (range == 0) {
		double mag = min;
		if (mag < 0) mag = -mag;
		sl_min = min;
		sl_max = min + mag;
	} else {
		sl_min = min;
		sl_max = max;
	}
	return sl_min;
}

/** Current maximum valid value for all Breaks Slider controls. We
 ensure that max-min in never zero.  Also, min cannot be below zero
 unless actual data points already go below zero. */
double CatClassifPanel::GetBrkSliderMax()
{
	double min = cc_data.uniform_dist_min;
	double max = cc_data.uniform_dist_max;
	double range = max-min;
	double sl_min;
	double sl_max;
	if (min == 0 && max == 0) {
		sl_min = 0;
		sl_max = 1;
	} else if (range == 0) {
		double mag = min;
		if (mag < 0) mag = -mag;
		sl_min = min;
		sl_max = min + mag;
	} else {
		sl_min = min;
		sl_max = max;
	}
	return sl_max;
}

/** If brk==-1, then disable slider */
void CatClassifPanel::SetSliderFromBreak(int brk)
{
	if (!all_init) return;
	if (brk < 0 || brk > cc_data.num_cats-2) {
		min_lbl->SetLabelText(" ");
		max_lbl->SetLabelText(" ");
		brk_slider->SetValue((brk_slider->GetMax()-brk_slider->GetMin())/2);
		brk_slider->Disable();
		return;
	}
	brk_slider->Enable();
	double min = GetBrkSliderMin();
	double max = GetBrkSliderMax();
	// max-min gauranteed to not be zero!
	min_lbl->SetLabelText(GenUtils::DblToStr(min));
	max_lbl->SetLabelText(GenUtils::DblToStr(max));
	
	double sl_pos_min = (double) brk_slider->GetMin();
	double sl_pos_max = (double) brk_slider->GetMax();
	double diff = cc_data.breaks[brk] - min;
	double p = sl_pos_min + (sl_pos_max-sl_pos_min)*(diff/(max-min));
	brk_slider->SetValue((int) p);
}

bool CatClassifPanel::IsDuplicateTitle(const wxString& title)
{
	wxString ttl(title);
	ttl.Trim(false);
	ttl.Trim(true);
	int num_cl = cur_cats_choice->GetCount();
	for (int i=0; i<num_cl; i++) {
		if (ttl == cur_cats_choice->GetString(i)) return true;
	}
	return false;
}

wxString CatClassifPanel::GetDefaultTitle(const wxString& field_name,
										  int field_tm)
{
	int max_tries = 500;
	int cur_try = 1;
	wxString ret_title_base("Custom Breaks");
	if (table_int->ColNameExists(field_name)) {
		ret_title_base << " (" << field_name;
		if (table_int->IsColTimeVariant(field_name)) {
			ret_title_base << ", ";
			ret_title_base << project->GetTableInt()->GetTimeString(field_tm);
		}
		ret_title_base << ")";
	}
	wxString ret_title = ret_title_base;
	while (IsDuplicateTitle(ret_title) && cur_try <= max_tries) {
		ret_title = ret_title_base;
		ret_title << " " << cur_try++;
	}
	return ret_title;
}

/** Return true if and only if current category has no interested clients. */
bool CatClassifPanel::IsOkToDelete(const wxString& custom_cat_title)
{
	CatClassifState* ccs =
	cat_classif_manager->FindClassifState(custom_cat_title);
	if (!ccs) return false;
	return ccs->GetNumberObservers() == 0;
}

/** If cc_state !=0, copy over current cc_data values and call
 notifyObservers. */
void CatClassifPanel::UpdateCCState()
{
	if (!cc_state) return;
	cc_state->SetCatClassif(cc_data);
	cc_state->notifyObservers();
	if (project->GetSaveButtonManager()) {
		project->GetSaveButtonManager()->SetMetaDataSaveNeeded(true);
	}
}

void CatClassifPanel::update(TableState* o)
{
	LOG_MSG("In CatClassifPanel::update(TableState* o)");
	InitAssocVarChoices();
	InitPreviewVarChoices();
}

BEGIN_EVENT_TABLE(CatClassifFrame, TemplateFrame)
	EVT_ACTIVATE(CatClassifFrame::OnActivate)
END_EVENT_TABLE()

CatClassifFrame::CatClassifFrame(wxFrame *parent, Project* project,
								 const wxString& title, const wxPoint& pos,
								 const wxSize& size, const long style)
: TemplateFrame(parent, project, title, pos, size, style)
{
	LOG_MSG("Entering CatClassifFrame::CatClassifFrame");
	
	/// START: original wxSpliterWindow desgin
	/*
	wxSplitterWindow* spliter_vert = 
		new wxSplitterWindow(this, -1, wxDefaultPosition, wxDefaultSize,
							 wxSP_3D|wxSP_LIVE_UPDATE|wxCLIP_CHILDREN);
	canvas = new CatClassifHistCanvas(spliter_vert, this, project);
	template_canvas = canvas;
	panel = new CatClassifPanel(project, canvas, spliter_vert, wxID_ANY);
	canvas->template_frame = this;
	panel->template_frame = this;
	spliter_vert->SplitVertically(panel, canvas);
	spliter_vert->SetMinimumPaneSize(20);
	SetMinSize(wxSize(600,500));
	 */
	/// END: orginal wxSpliterWindow desgin

	
	/// START: wxBoxSizer desgin
	wxPanel* histo_panel = new wxPanel(this);
	wxStaticText* preview_var_text = new wxStaticText(histo_panel, wxID_ANY,
													  "Preview Var.");
	wxChoice* preview_var_choice = new wxChoice(histo_panel,
									  XRCID("ID_PREVIEW_VAR_CHOICE"),
									  wxDefaultPosition,
									  wxSize(120, -1));
	Connect(XRCID("ID_PREVIEW_VAR_CHOICE"), wxEVT_CHOICE,
			wxCommandEventHandler(CatClassifFrame::OnPreviewVarChoice));

	wxChoice* preview_var_tm_choice = new wxChoice(histo_panel,
									  XRCID("ID_PREVIEW_VAR_TM_CHOICE"),
									  wxDefaultPosition,
									  wxSize(70, -1));
	Connect(XRCID("ID_PREVIEW_VAR_TM_CHOICE"), wxEVT_CHOICE,
			wxCommandEventHandler(CatClassifFrame::OnPreviewVarTmChoice));
	
	wxBoxSizer* histo_h_szr = new wxBoxSizer(wxHORIZONTAL);
	histo_h_szr->Add(preview_var_text, 0, wxALIGN_CENTER_VERTICAL);
	histo_h_szr->AddSpacer(3);
	histo_h_szr->Add(preview_var_choice, 0, wxALIGN_CENTER_VERTICAL);
	histo_h_szr->AddSpacer(5);
	histo_h_szr->Add(preview_var_tm_choice, 0, wxALIGN_CENTER_VERTICAL);
	
	wxCheckBox* sync_vars_chk = new wxCheckBox(histo_panel,
											   XRCID("ID_SYNC_VARS_CHK"),
											   "same as Assoc. Var.");
	sync_vars_chk->SetValue(true);
	Connect(XRCID("ID_SYNC_VARS_CHK"), wxEVT_CHECKBOX,
			wxCommandEventHandler(CatClassifFrame::OnSyncVarsChk));

	wxBoxSizer* histo_v_szr = new wxBoxSizer(wxVERTICAL);
	histo_v_szr->Add(histo_h_szr, 1, wxEXPAND|wxALIGN_CENTER_HORIZONTAL);
	histo_v_szr->Add(sync_vars_chk, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 5);
	
	histo_panel->SetSizer(histo_v_szr);
	
	canvas = new CatClassifHistCanvas(this, this, project, wxDefaultPosition,
									  wxSize(300, -1));
	canvas->SetMinSize(wxSize(100, -1));
	template_canvas = canvas;
	panel = new CatClassifPanel(project, canvas, this, 
								preview_var_choice, preview_var_tm_choice,
								sync_vars_chk, wxID_ANY,
								wxDefaultPosition, wxDefaultSize);
	canvas->template_frame = this;
	panel->template_frame = this;

	wxBoxSizer* r_sizer = new wxBoxSizer(wxVERTICAL);
	r_sizer->Add(canvas, 1, wxEXPAND);
	wxBoxSizer* spacer_r_300 = new wxBoxSizer(wxHORIZONTAL);
	spacer_r_300->AddSpacer(300);
	r_sizer->Add(spacer_r_300, 0);
	r_sizer->Add(histo_panel, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);
	
	wxBoxSizer* top_sizer = new wxBoxSizer(wxHORIZONTAL);
	top_sizer->Add(panel, 0);
	wxBoxSizer* spacer_v_500 = new wxBoxSizer(wxVERTICAL);
	spacer_v_500->AddSpacer(500);
	top_sizer->Add(spacer_v_500, 0);
	top_sizer->Add(r_sizer, 1, wxEXPAND);
	
	wxColour panel_color = panel->GetBackgroundColour();
	SetBackgroundColour(panel_color);
	canvas->SetCanvasBackgroundColor(panel_color);

	SetSizerAndFit(top_sizer);
	/// END: wxBoxSizer desgin	
	
	DisplayStatusBar(true);
	SetTitle(template_canvas->GetCanvasTitle());
	Show(true);
	LOG_MSG("Exiting CatClassifFrame::CatClassifFrame");
}

///MMM: Sort out in all Frames: what should be in the destructor?
CatClassifFrame::~CatClassifFrame()
{
	if (HasCapture()) ReleaseMouse();
	DeregisterAsActive();
	frames_manager->removeObserver(this);
}

void CatClassifFrame::OnActivate(wxActivateEvent& event)
{
	LOG_MSG("In CatClassifFrame::OnActivate");
	if (event.GetActive()) {
		RegisterAsActive("CatClassifFrame", GetTitle());
	}
    if ( event.GetActive() && template_canvas ) template_canvas->SetFocus();
}

void CatClassifFrame::OnPreviewVarChoice(wxCommandEvent& event)
{
	panel->OnPreviewVarChoice(event);
}

void CatClassifFrame::OnPreviewVarTmChoice(wxCommandEvent& event)
{
	panel->OnPreviewVarTmChoice(event);
}

void CatClassifFrame::OnSyncVarsChk(wxCommandEvent& event)
{
	panel->OnSyncVarsChk(event);
}

/** Implementation of TimeStateObserver interface */
void CatClassifFrame::update(TimeState* o)
{
	LOG_MSG("In CatClassifFrame::update(TimeState* o)");
	template_canvas->TimeChange();
}

CatClassifState* CatClassifFrame::PromptNew(const CatClassifDef& ccd,
											const wxString& suggested_title,
											const wxString& field_name,
											int field_tm)
{
	return panel->PromptNew(ccd, suggested_title, field_name, field_tm);
}

