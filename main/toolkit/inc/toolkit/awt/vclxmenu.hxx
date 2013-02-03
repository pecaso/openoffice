/**************************************************************
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 *************************************************************/

#ifndef _TOOLKIT_AWT_VCLXMENU_HXX_
#define _TOOLKIT_AWT_VCLXMENU_HXX_

#include <toolkit/dllapi.h>
#include <toolkit/helper/listenermultiplexer.hxx>

#include <com/sun/star/awt/XMenuBar.hpp>
#include <com/sun/star/awt/XPopupMenu.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/lang/XTypeProvider.hpp>
#include <com/sun/star/lang/XUnoTunnel.hpp>

#include <cppuhelper/weak.hxx>
#include <osl/mutex.hxx>
#include <tools/link.hxx>
#include <tools/list.hxx>

namespace css = ::com::sun::star;

class Menu;
class MenuBar;
class VclSimpleEvent;

DECLARE_LIST( PopupMenuRefList, css::uno::Reference< css::awt::XPopupMenu >* )

class TOOLKIT_DLLPUBLIC VCLXMenu : public css::awt::XMenuBar,
                                   public css::awt::XPopupMenu,
                                   public css::lang::XServiceInfo,
                                   public css::lang::XTypeProvider,
                                   public css::lang::XUnoTunnel,
                                   public ::cppu::OWeakObject
{
private:
    ::osl::Mutex                maMutex;
    Menu*                       mpMenu;
    MenuListenerMultiplexer     maMenuListeners;
    PopupMenuRefList            maPopupMenueRefs;

protected:
    ::osl::Mutex&               GetMutex() { return maMutex; }

    DECL_LINK( MenuEventListener, VclSimpleEvent* );

    void ImplCreateMenu( sal_Bool bPopup );

public:
    VCLXMenu();
    VCLXMenu( Menu* pMenu );
    ~VCLXMenu();

    Menu*    GetMenu() const { return mpMenu; }
    sal_Bool IsPopupMenu() const;

    // css::uno::XInterface
    css::uno::Any SAL_CALL queryInterface( const css::uno::Type & rType ) throw(css::uno::RuntimeException);
    void SAL_CALL acquire() throw() { OWeakObject::acquire(); }
    void SAL_CALL release() throw() { OWeakObject::release(); }

    // css::lang::XUnoTunnel
    static const css::uno::Sequence< sal_Int8 >&  GetUnoTunnelId() throw();
    static VCLXMenu* GetImplementation( const css::uno::Reference< css::uno::XInterface >& rxIFace ) throw();
    sal_Int64 SAL_CALL getSomething( const css::uno::Sequence< sal_Int8 >& rIdentifier ) throw(css::uno::RuntimeException);

    // css::lang::XTypeProvider
    css::uno::Sequence< css::uno::Type > SAL_CALL getTypes() throw(css::uno::RuntimeException);
    css::uno::Sequence< sal_Int8 > SAL_CALL getImplementationId() throw(css::uno::RuntimeException);

    // css::awt::XMenu
    virtual void SAL_CALL addMenuListener( const css::uno::Reference< css::awt::XMenuListener >& xListener ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL removeMenuListener( const css::uno::Reference< css::awt::XMenuListener >& xListener ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL insertItem( ::sal_Int16 nItemId, const ::rtl::OUString& aText, ::sal_Int16 nItemStyle, ::sal_Int16 nItemPos ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL removeItem( ::sal_Int16 nItemPos, ::sal_Int16 nCount ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL clear(  ) throw (css::uno::RuntimeException);
    virtual ::sal_Int16 SAL_CALL getItemCount(  ) throw (css::uno::RuntimeException);
    virtual ::sal_Int16 SAL_CALL getItemId( ::sal_Int16 nItemPos ) throw (css::uno::RuntimeException);
    virtual ::sal_Int16 SAL_CALL getItemPos( ::sal_Int16 nItemId ) throw (css::uno::RuntimeException);
    virtual css::awt::MenuItemType SAL_CALL getItemType( ::sal_Int16 nItemPos ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL enableItem( ::sal_Int16 nItemId, ::sal_Bool bEnable ) throw (css::uno::RuntimeException);
    virtual ::sal_Bool SAL_CALL isItemEnabled( ::sal_Int16 nItemId ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL hideDisabledEntries( ::sal_Bool bHide ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL enableAutoMnemonics( ::sal_Bool bEnable ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL setItemText( ::sal_Int16 nItemId, const ::rtl::OUString& aText ) throw (css::uno::RuntimeException);
    virtual ::rtl::OUString SAL_CALL getItemText( ::sal_Int16 nItemId ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL setCommand( ::sal_Int16 nItemId, const ::rtl::OUString& aCommand ) throw (css::uno::RuntimeException);
    virtual ::rtl::OUString SAL_CALL getCommand( ::sal_Int16 nItemId ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL setHelpCommand( ::sal_Int16 nItemId, const ::rtl::OUString& aCommand ) throw (css::uno::RuntimeException);
    virtual ::rtl::OUString SAL_CALL getHelpCommand( ::sal_Int16 nItemId ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL setHelpText( ::sal_Int16 nItemId, const ::rtl::OUString& sHelpText ) throw (css::uno::RuntimeException);
    virtual ::rtl::OUString SAL_CALL getHelpText( ::sal_Int16 nItemId ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL setTipHelpText( ::sal_Int16 nItemId, const ::rtl::OUString& sTipHelpText ) throw (css::uno::RuntimeException);
    virtual ::rtl::OUString SAL_CALL getTipHelpText( ::sal_Int16 nItemId ) throw (css::uno::RuntimeException);
    virtual ::sal_Bool SAL_CALL isPopupMenu(  ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL setPopupMenu( ::sal_Int16 nItemId, const css::uno::Reference< css::awt::XPopupMenu >& aPopupMenu ) throw (css::uno::RuntimeException);
    virtual css::uno::Reference< css::awt::XPopupMenu > SAL_CALL getPopupMenu( ::sal_Int16 nItemId ) throw (css::uno::RuntimeException);

    // css::awt::XPopupMenu
    virtual void SAL_CALL insertSeparator( ::sal_Int16 nItemPos ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL setDefaultItem( ::sal_Int16 nItemId ) throw (css::uno::RuntimeException);
    virtual ::sal_Int16 SAL_CALL getDefaultItem(  ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL checkItem( ::sal_Int16 nItemId, ::sal_Bool bCheck ) throw (css::uno::RuntimeException);
    virtual ::sal_Bool SAL_CALL isItemChecked( ::sal_Int16 nItemId ) throw (css::uno::RuntimeException);
    virtual ::sal_Int16 SAL_CALL execute( const css::uno::Reference< css::awt::XWindowPeer >& Parent, const css::awt::Point& Position, ::sal_Int16 Direction ) throw (css::uno::RuntimeException);
    virtual ::sal_Bool SAL_CALL isInExecute(  ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL endExecute(  ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL setAcceleratorKeyEvent( ::sal_Int16 nItemId, const css::awt::KeyEvent& aKeyEvent ) throw (css::uno::RuntimeException);
    virtual css::awt::KeyEvent SAL_CALL getAcceleratorKeyEvent( ::sal_Int16 nItemId ) throw (css::uno::RuntimeException);
    virtual void SAL_CALL setItemImage( ::sal_Int16 nItemId, const css::uno::Reference< css::graphic::XGraphic >& xGraphic, ::sal_Bool bScale ) throw (css::uno::RuntimeException);
    virtual css::uno::Reference< css::graphic::XGraphic > SAL_CALL getItemImage( ::sal_Int16 nItemId ) throw (css::uno::RuntimeException);

    // css::lang::XServiceInfo
    virtual ::rtl::OUString SAL_CALL getImplementationName(  ) throw (css::uno::RuntimeException);
    virtual ::sal_Bool SAL_CALL supportsService( const ::rtl::OUString& ServiceName ) throw (css::uno::RuntimeException);
    virtual css::uno::Sequence< ::rtl::OUString > SAL_CALL getSupportedServiceNames(  ) throw (css::uno::RuntimeException);
};

class TOOLKIT_DLLPUBLIC VCLXMenuBar : public VCLXMenu
{
public:
    VCLXMenuBar();
    VCLXMenuBar( MenuBar* pMenuBar );
};

class TOOLKIT_DLLPUBLIC VCLXPopupMenu : public VCLXMenu
{
public:
    VCLXPopupMenu();
};

#endif
