// Copyright (c) 2005, Rodrigo Braz Monteiro
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

/// @file dialog_search_replace.cpp
/// @brief Find and Search/replace dialogue box and logic
/// @ingroup secondary_ui
///

#include "config.h"

#include "dialog_search_replace.h"

#include "ass_dialogue.h"
#include "ass_file.h"
#include "compat.h"
#include "include/aegisub/context.h"
#include "options.h"
#include "selection_controller.h"
#include "text_selection_controller.h"
#include "subs_grid.h"
#include "utils.h"

#include <libaegisub/of_type_adaptor.h>

#include <functional>

#include <wx/button.h>
#include <wx/msgdlg.h>
#include <wx/regex.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/string.h>

enum {
	BUTTON_FIND_NEXT,
	BUTTON_REPLACE_NEXT,
	BUTTON_REPLACE_ALL,
	CHECK_MATCH_CASE,
	CHECK_REGEXP,
	CHECK_UPDATE_VIDEO
};

DialogSearchReplace::DialogSearchReplace(agi::Context* c, bool withReplace)
: wxDialog(c->parent, -1, withReplace ? _("Replace") : _("Find"))
, hasReplace(withReplace)
{
	wxSizer *FindSizer = new wxFlexGridSizer(2,2,5,15);
	FindEdit = new wxComboBox(this,-1,"",wxDefaultPosition,wxSize(300,-1),lagi_MRU_wxAS("Find"),wxCB_DROPDOWN);
	if (!FindEdit->IsListEmpty())
		FindEdit->SetSelection(0);
	FindSizer->Add(new wxStaticText(this,-1,_("Find what:")),0,wxRIGHT | wxALIGN_CENTER_VERTICAL,0);
	FindSizer->Add(FindEdit,0,wxRIGHT,0);
	if (hasReplace) {
		ReplaceEdit = new wxComboBox(this,-1,"",wxDefaultPosition,wxSize(300,-1),lagi_MRU_wxAS("Replace"),wxCB_DROPDOWN);
		FindSizer->Add(new wxStaticText(this,-1,_("Replace with:")),0,wxRIGHT | wxALIGN_CENTER_VERTICAL,0);
		FindSizer->Add(ReplaceEdit,0,wxRIGHT,0);
		if (!ReplaceEdit->IsListEmpty())
			ReplaceEdit->SetSelection(0);
	}

	wxSizer *OptionsSizer = new wxBoxSizer(wxVERTICAL);
	CheckMatchCase = new wxCheckBox(this,CHECK_MATCH_CASE,_("&Match case"));
	CheckRegExp = new wxCheckBox(this,CHECK_MATCH_CASE,_("&Use regular expressions"));
	CheckMatchCase->SetValue(OPT_GET("Tool/Search Replace/Match Case")->GetBool());
	CheckRegExp->SetValue(OPT_GET("Tool/Search Replace/RegExp")->GetBool());
	OptionsSizer->Add(CheckMatchCase,0,wxBOTTOM,5);
	OptionsSizer->Add(CheckRegExp,0,wxBOTTOM,5);

	// Limits sizer
	wxString field[] = { _("Text"), _("Style"), _("Actor"), _("Effect") };
	wxString affect[] = { _("All rows"), _("Selected rows") };
	Field = new wxRadioBox(this,-1,_("In Field"),wxDefaultPosition,wxDefaultSize,countof(field), field);
	Affect = new wxRadioBox(this,-1,_("Limit to"),wxDefaultPosition,wxDefaultSize,countof(affect), affect);
	wxSizer *LimitSizer = new wxBoxSizer(wxHORIZONTAL);
	LimitSizer->Add(Field,1,wxEXPAND | wxRIGHT,5);
	LimitSizer->Add(Affect,0,wxEXPAND | wxRIGHT,0);
	Field->SetSelection(OPT_GET("Tool/Search Replace/Field")->GetInt());
	Affect->SetSelection(OPT_GET("Tool/Search Replace/Affect")->GetInt());

	// Left sizer
	wxSizer *LeftSizer = new wxBoxSizer(wxVERTICAL);
	LeftSizer->Add(FindSizer,0,wxBOTTOM,10);
	LeftSizer->Add(OptionsSizer,0,wxBOTTOM,5);
	LeftSizer->Add(LimitSizer,0,wxEXPAND | wxBOTTOM,0);

	// Buttons
	wxSizer *ButtonSizer = new wxBoxSizer(wxVERTICAL);
	wxButton *FindNext = new wxButton(this,BUTTON_FIND_NEXT,_("&Find next"));
	FindNext->SetDefault();
	ButtonSizer->Add(FindNext,0,wxEXPAND | wxBOTTOM,3);
	if (hasReplace) {
		ButtonSizer->Add(new wxButton(this,BUTTON_REPLACE_NEXT,_("Replace &next")),0,wxEXPAND | wxBOTTOM,3);
		ButtonSizer->Add(new wxButton(this,BUTTON_REPLACE_ALL,_("Replace &all")),0,wxEXPAND | wxBOTTOM,3);
	}
	ButtonSizer->Add(new wxButton(this,wxID_CANCEL),0,wxEXPAND | wxBOTTOM,20);

	// Main sizer
	wxSizer *MainSizer = new wxBoxSizer(wxHORIZONTAL);
	MainSizer->Add(LeftSizer,0,wxEXPAND | wxALL,5);
	MainSizer->Add(ButtonSizer,0,wxEXPAND | wxALL,5);
	SetSizerAndFit(MainSizer);
	CenterOnParent();

	Search.OnDialogOpen();

	Bind(wxEVT_COMMAND_BUTTON_CLICKED, std::bind(&DialogSearchReplace::FindReplace, this, 0), BUTTON_FIND_NEXT);
	Bind(wxEVT_COMMAND_BUTTON_CLICKED, std::bind(&DialogSearchReplace::FindReplace, this, 1), BUTTON_REPLACE_NEXT);
	Bind(wxEVT_COMMAND_BUTTON_CLICKED, std::bind(&DialogSearchReplace::FindReplace, this, 2), BUTTON_REPLACE_ALL);
	Bind(wxEVT_SET_FOCUS, std::bind(&SearchReplaceEngine::SetFocus, &Search, true));
	Bind(wxEVT_KILL_FOCUS, std::bind(&SearchReplaceEngine::SetFocus, &Search, false));
}

DialogSearchReplace::~DialogSearchReplace() {
	UpdateSettings();
}

void DialogSearchReplace::UpdateSettings() {
	Search.isReg = CheckRegExp->IsChecked() && CheckRegExp->IsEnabled();
	Search.matchCase = CheckMatchCase->IsChecked();
	OPT_SET("Tool/Search Replace/Match Case")->SetBool(CheckMatchCase->IsChecked());
	OPT_SET("Tool/Search Replace/RegExp")->SetBool(CheckRegExp->IsChecked());
	OPT_SET("Tool/Search Replace/Field")->SetInt(Field->GetSelection());
	OPT_SET("Tool/Search Replace/Affect")->SetInt(Affect->GetSelection());
}

void DialogSearchReplace::FindReplace(int mode) {
	if (mode < 0 || mode > 2) return;

	wxString LookFor = FindEdit->GetValue();
	if (!LookFor) return;

	Search.isReg = CheckRegExp->IsChecked() && CheckRegExp->IsEnabled();
	Search.matchCase = CheckMatchCase->IsChecked();
	Search.LookFor = LookFor;
	Search.CanContinue = true;
	Search.affect = Affect->GetSelection();
	Search.field = Field->GetSelection();

	if (mode == 0) {
		Search.FindNext();
		if (hasReplace) {
			wxString ReplaceWith = ReplaceEdit->GetValue();
			Search.ReplaceWith = ReplaceWith;
			config::mru->Add("Replace", from_wx(ReplaceWith));
		}
	}
	else {
		wxString ReplaceWith = ReplaceEdit->GetValue();
		Search.ReplaceWith = ReplaceWith;
		if (mode == 1) Search.ReplaceNext();
		else Search.ReplaceAll();
		config::mru->Add("Replace", from_wx(ReplaceWith));
	}

	config::mru->Add("Find", from_wx(LookFor));
	UpdateDropDowns();
}

void DialogSearchReplace::UpdateDropDowns() {
	FindEdit->Freeze();
	FindEdit->Clear();
	FindEdit->Append(lagi_MRU_wxAS("Find"));
	if (!FindEdit->IsListEmpty())
		FindEdit->SetSelection(0);
	FindEdit->Thaw();

	if (hasReplace) {
		ReplaceEdit->Freeze();
		ReplaceEdit->Clear();
		ReplaceEdit->Append(lagi_MRU_wxAS("Replace"));
		if (!ReplaceEdit->IsListEmpty())
			ReplaceEdit->SetSelection(0);
		ReplaceEdit->Thaw();
	}
}

SearchReplaceEngine::SearchReplaceEngine()
: curLine(0)
, pos(0)
, matchLen(0)
, replaceLen(0)
, LastWasFind(true)
, hasReplace(false)
, isReg(false)
, matchCase(false)
, CanContinue(false)
, hasFocus(false)
, field(0)
, affect(0)
, context(0)
{
}

void SearchReplaceEngine::FindNext() {
	ReplaceNext(false);
}

static boost::flyweight<wxString> *get_text(AssDialogue *cur, int field) {
	if (field == 0) return &cur->Text;
	else if (field == 1) return &cur->Style;
	else if (field == 2) return &cur->Actor;
	else if (field == 3) return &cur->Effect;
	else throw wxString("Invalid field");
}

void SearchReplaceEngine::ReplaceNext(bool DoReplace) {
	if (!CanContinue) {
		OpenDialog(DoReplace);
		return;
	}

	wxArrayInt sels = context->subsGrid->GetSelection();
	int firstLine = 0;
	if (sels.Count() > 0) firstLine = sels[0];
	// if selection has changed reset values
	if (firstLine != curLine) {
		curLine = firstLine;
		LastWasFind = true;
		pos = 0;
		matchLen = 0;
		replaceLen = 0;
	}

	// Setup
	int start = curLine;
	int nrows = context->subsGrid->GetRows();
	bool found = false;
	size_t tempPos;
	int regFlags = wxRE_ADVANCED;
	if (!matchCase) {
		if (isReg) regFlags |= wxRE_ICASE;
		else LookFor.MakeLower();
	}

	// Search for it
	boost::flyweight<wxString> *Text = nullptr;
	while (!found) {
		Text = get_text(context->subsGrid->GetDialogue(curLine), field);
		if (DoReplace && LastWasFind)
			tempPos = pos;
		else
			tempPos = pos+replaceLen;

		if (isReg) {
			wxRegEx regex (LookFor,regFlags);
			if (regex.IsValid()) {
				if (regex.Matches(Text->get().Mid(tempPos))) {
					size_t match_start;
					regex.GetMatch(&match_start,&matchLen,0);
					pos = match_start + tempPos;
					found = true;
				}
			}
		}
		else {
			wxString src = Text->get().Mid(tempPos);
			if (!matchCase) src.MakeLower();
			int textPos = src.Find(LookFor);
			if (textPos != -1) {
				pos = tempPos+textPos;
				found = true;
				matchLen = LookFor.Length();
			}
		}

		// Didn't find, go to next line
		if (!found) {
			curLine++;
			pos = 0;
			matchLen = 0;
			replaceLen = 0;
			if (curLine == nrows) curLine = 0;
			if (curLine == start) break;
		}
	}

	if (found) {
		if (!DoReplace)
			replaceLen = matchLen;
		else {
			if (isReg) {
				wxString toReplace = Text->get().Mid(pos,matchLen);
				wxRegEx regex(LookFor,regFlags);
				regex.ReplaceFirst(&toReplace,ReplaceWith);
				*Text = Text->get().Left(pos) + toReplace + Text->get().Mid(pos+matchLen);
				replaceLen = toReplace.Length();
			}
			else {
				*Text = Text->get().Left(pos) + ReplaceWith + Text->get().Mid(pos+matchLen);
				replaceLen = ReplaceWith.Length();
			}

			context->ass->Commit(_("replace"), AssFile::COMMIT_DIAG_TEXT);
		}

		context->subsGrid->SelectRow(curLine,false);
		context->subsGrid->MakeCellVisible(curLine,0);
		if (field == 0) {
			context->selectionController->SetActiveLine(context->subsGrid->GetDialogue(curLine));
			context->textSelectionController->SetSelection(pos, pos + replaceLen);
		}

		// hAx to prevent double match on style/actor
		if (field != 0) replaceLen = 99999;
	}
	LastWasFind = !DoReplace;
}

void SearchReplaceEngine::ReplaceAll() {
	size_t count = 0;

	int regFlags = wxRE_ADVANCED;
	if (!matchCase)
		regFlags |= wxRE_ICASE;
	wxRegEx reg;
	if (isReg)
		reg.Compile(LookFor, regFlags);

	SubtitleSelection const& sel = context->selectionController->GetSelectedSet();
	bool hasSelection = !sel.empty();
	bool inSel = affect == 1;

	for (auto diag : context->ass->Line | agi::of_type<AssDialogue>()) {
		if (inSel && hasSelection && !sel.count(diag))
			continue;

		boost::flyweight<wxString> *Text = get_text(diag, field);

		if (isReg) {
			if (reg.Matches(*Text)) {
				size_t start, len;
				reg.GetMatch(&start, &len);

				// A zero length match (such as '$') will always be replaced
				// maxMatches times, which is almost certainly not what the user
				// wanted, so limit it to one replacement in that situation
				wxString repl(*Text);
				count += reg.Replace(&repl, ReplaceWith, len > 0 ? 1000 : 1);
				*Text = repl;
			}
		}
		else {
			if (!Search.matchCase) {
				bool replaced = false;
				wxString Left, Right = *Text;
				size_t pos = 0;
				Left.reserve(Right.size());
				while (pos + LookFor.size() <= Right.size()) {
					if (Right.Mid(pos, LookFor.size()).CmpNoCase(LookFor) == 0) {
						Left.Append(Right.Left(pos)).Append(ReplaceWith);
						Right = Right.Mid(pos + LookFor.Len());
						++count;
						replaced = true;
						pos = 0;
					}
					else {
						pos++;
					}
				}
				if (replaced) {
					*Text = Left + Right;
				}
			}
			else if(Text->get().Contains(LookFor)) {
				wxString repl(*Text);
				count += repl.Replace(LookFor, ReplaceWith);
				*Text = repl;
			}
		}
	}

	if (count > 0) {
		context->ass->Commit(_("replace"), AssFile::COMMIT_DIAG_TEXT);
		wxMessageBox(wxString::Format(_("%i matches were replaced."), (int)count));
	}
	else {
		wxMessageBox(_("No matches found."));
	}
	LastWasFind = false;
}

void SearchReplaceEngine::OnDialogOpen() {
	wxArrayInt sels = context->subsGrid->GetSelection();
	curLine = 0;
	if (sels.Count() > 0) curLine = sels[0];

	LastWasFind = true;
	pos = 0;
	matchLen = 0;
	replaceLen = 0;
}

void SearchReplaceEngine::OpenDialog(bool replace) {
	static DialogSearchReplace *diag = nullptr;

	// already opened
	if (diag) {
		// it's the right type so give focus
		if(replace == hasReplace) {
			diag->FindEdit->SetFocus();
			diag->Show();
			OnDialogOpen();
			return;
		}
		// wrong type - destroy and create the right one
		diag->Destroy();
	}
	// create new one
	diag = new DialogSearchReplace(context, replace);
	diag->FindEdit->SetFocus();
	diag->Show();
	hasReplace = replace;
}

SearchReplaceEngine Search;
