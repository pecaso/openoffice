#**************************************************************
#  
#  Licensed to the Apache Software Foundation (ASF) under one
#  or more contributor license agreements.  See the NOTICE file
#  distributed with this work for additional information
#  regarding copyright ownership.  The ASF licenses this file
#  to you under the Apache License, Version 2.0 (the
#  "License"); you may not use this file except in compliance
#  with the License.  You may obtain a copy of the License at
#  
#    http://www.apache.org/licenses/LICENSE-2.0
#  
#  Unless required by applicable law or agreed to in writing,
#  software distributed under the License is distributed on an
#  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
#  specific language governing permissions and limitations
#  under the License.
#  
#**************************************************************



$(eval $(call gb_AllLangResTarget_AllLangResTarget,cui))

$(eval $(call gb_AllLangResTarget_set_reslocation,cui,svx))

$(eval $(call gb_AllLangResTarget_add_srs,cui,cui/res))

$(eval $(call gb_SrsTarget_SrsTarget,cui/res))

$(eval $(call gb_SrsTarget_set_include,cui/res,\
	$$(INCLUDE) \
	-I$(SRCDIR)/cui/source/inc \
))

# add src files here (complete path relative to repository root)
$(eval $(call gb_SrsTarget_add_files,cui/res,\
	cui/source/customize/acccfg.src \
	cui/source/customize/cfg.src \
	cui/source/customize/eventdlg.src \
	cui/source/customize/macropg.src \
	cui/source/customize/selector.src \
	cui/source/dialogs/about.src \
	cui/source/dialogs/colorpicker.src \
	cui/source/dialogs/charmap.src \
	cui/source/dialogs/commonlingui.src \
	cui/source/dialogs/cuiimapdlg.src \
	cui/source/dialogs/dlgname.src \
	cui/source/dialogs/fmsearch.src \
	cui/source/dialogs/gallery.src \
	cui/source/dialogs/grfflt.src \
	cui/source/dialogs/hangulhanjadlg.src \
	cui/source/dialogs/hyphen.src \
	cui/source/dialogs/hlmarkwn.src \
	cui/source/dialogs/hyperdlg.src \
	cui/source/dialogs/iconcdlg.src \
	cui/source/dialogs/insrc.src \
	cui/source/dialogs/multipat.src \
	cui/source/dialogs/newtabledlg.src \
	cui/source/dialogs/passwdomdlg.src \
	cui/source/dialogs/postdlg.src \
	cui/source/dialogs/scriptdlg.src \
	cui/source/dialogs/sdrcelldlg.src \
	cui/source/dialogs/showcols.src \
	cui/source/dialogs/SpellDialog.src \
	cui/source/dialogs/splitcelldlg.src \
	cui/source/dialogs/srchxtra.src \
	cui/source/dialogs/svuidlg.src \
	cui/source/dialogs/tbxform.src \
	cui/source/dialogs/thesdlg.src \
	cui/source/dialogs/zoom.src \
	cui/source/options/connpooloptions.src \
	cui/source/options/dbregister.src \
	cui/source/options/doclinkdialog.src \
	cui/source/options/fontsubs.src \
	cui/source/options/internationaloptions.src \
	cui/source/options/optaccessibility.src \
	cui/source/options/optasian.src \
	cui/source/options/optchart.src \
	cui/source/options/optcolor.src \
	cui/source/options/optctl.src \
	cui/source/options/optdict.src \
	cui/source/options/optfltr.src \
	cui/source/options/optgdlg.src \
	cui/source/options/optgenrl.src \
	cui/source/options/opthtml.src \
	cui/source/options/optimprove.src \
	cui/source/options/optinet2.src \
	cui/source/options/optjava.src \
	cui/source/options/optjsearch.src \
	cui/source/options/optlingu.src \
	cui/source/options/optmemory.src \
	cui/source/options/optpath.src \
	cui/source/options/optsave.src \
	cui/source/options/optupdt.src \
	cui/source/options/readonlyimage.src \
	cui/source/options/securityoptions.src \
	cui/source/options/treeopt.src \
	cui/source/options/webconninfo.src \
	cui/source/tabpages/align.src \
	cui/source/tabpages/autocdlg.src \
	cui/source/tabpages/backgrnd.src \
	cui/source/tabpages/bbdlg.src \
	cui/source/tabpages/border.src \
	cui/source/tabpages/chardlg.src \
	cui/source/tabpages/connect.src \
	cui/source/tabpages/dstribut.src \
	cui/source/tabpages/frmdirlbox.src \
	cui/source/tabpages/grfpage.src \
	cui/source/tabpages/labdlg.src \
	cui/source/tabpages/macroass.src \
	cui/source/tabpages/measure.src \
	cui/source/tabpages/numfmt.src \
	cui/source/tabpages/numpages.src \
	cui/source/tabpages/page.src \
	cui/source/tabpages/paragrph.src \
	cui/source/tabpages/strings.src \
	cui/source/tabpages/swpossizetabpage.src \
	cui/source/tabpages/tabarea.src \
	cui/source/tabpages/tabline.src \
	cui/source/tabpages/tabstpge.src \
	cui/source/tabpages/textanim.src \
	cui/source/tabpages/textattr.src \
	cui/source/tabpages/transfrm.src \
))

# vim: set noet sw=4 ts=4:
