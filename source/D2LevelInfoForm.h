/*
    Diablo II Character Editor
    Copyright (C) 2000-2003  Burton Tsang
    Copyright (C) 2021 Walter Couto

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
//---------------------------------------------------------------------------

#pragma once

#include "d2ce\CharacterConstants.h"
#include "resource.h"

//---------------------------------------------------------------------------
class CD2LevelInfoForm : public CDialogEx
{
    DECLARE_DYNAMIC(CD2LevelInfoForm)

public:
    CD2LevelInfoForm(CWnd* pParent = nullptr);   // standard constructor
    virtual ~CD2LevelInfoForm();

    // Dialog Data
    enum { IDD = IDD_D2LEVELINFO_DIALOG };


protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual void OnOK();
    virtual void OnCancel();

    DECLARE_MESSAGE_MAP()

    void FillCells();

public:
    CListCtrl LevelInfoGrid;
    virtual BOOL OnInitDialog();

private:
    d2ce::EnumCharVersion Version = d2ce::APP_CHAR_VERSION;
    BOOL Modal = FALSE;

public:
    virtual INT_PTR DoModal();
    BOOL Show(CWnd* pParentWnd = NULL);
    void ResetVersion(d2ce::EnumCharVersion version);
};
//---------------------------------------------------------------------------
