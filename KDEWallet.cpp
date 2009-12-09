/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is KDE Wallet password manager storage.
 *
 * The Initial Developer of the Original Code is
 * Guillermo Molina <guillermoadrianmolina@hotmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* This project is based on previous work made in:
	firefox gnome keyring extension
*/

#include "KDEWallet.h"
#include "nsILoginInfo.h"

#include "nsIGenericFactory.h"
#include "nsMemory.h"
#include "nsICategoryManager.h"
#include "nsComponentManagerUtils.h"
#include "nsStringAPI.h"
#include "nsIXULAppInfo.h"
#include "nsXULAppAPI.h"
#include "nsServiceManagerUtils.h"
#include "nsIPropertyBag.h"
#include "nsIProperty.h"
#include "nsIVariant.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

PRLogModuleInfo *gKDEWalletLog;

/* Include the requried KDE headers. */
#include <kapplication.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <kwallet.h>

KWallet::Wallet *wallet = NULL; // A pointer to KWallet - it doesn't like be declared over and over inside a function.
KApplication *app; // A pointer to the KApplication app - can only declare it once or we'll core dump!

const char *kSaveDisabledHostsMapName = "SaveDisabledHosts";

const char *kHostnameAttr = "hostname";
const char *kFormSubmitURLAttr = "formSubmitURL";
const char *kHttpRealmAttr = "httpRealm";
const char *kUsernameFieldAttr = "usernameField";
const char *kPasswordFieldAttr = "passwordField";
const char *kUsernameAttr = "username";
const char *kPasswordAttr = "password";

/* Implementation file */
NS_IMPL_ISUPPORTS1(KDEWallet, nsILoginManagerStorage)

bool checkWallet() {
	if( !wallet ) {
		wallet =  KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous );
		if( !wallet )
			return false;
	}
	if( !wallet->hasFolder("Firefox") ) {
		if( !wallet->createFolder("Firefox") ) 
			return false;
	}
	if( !wallet->setFolder("Firefox") )
		return false;
	return true;
}

nsAutoString QtString2NSString ( const QString &qtString ) {
	return NS_ConvertUTF8toUTF16( qtString.toUtf8().data() );
}

QString generateWalletKey( const nsAString & aHostname,
			const nsAString & aActionURL,
			const nsAString & aHttpRealm,
			const nsAString & aUsername ) {
	QString key = NS_ConvertUTF16toUTF8(aUsername).get();
	key += ",";
	key += aActionURL.IsVoid() ? "" : NS_ConvertUTF16toUTF8(aActionURL).get();
	key += ",";
	key += aHttpRealm.IsVoid() ? "" : NS_ConvertUTF16toUTF8(aHttpRealm).get();
	key += ",";
	key += NS_ConvertUTF16toUTF8(aHostname).get();
	return key;
}

NS_IMETHODIMP KDEWallet::Init() {
	gKDEWalletLog = PR_NewLogModule("KDEWalletLog");

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::Init() Called") );
  
	/* KWallet requries a functioning KApplication or it will segfault */
	KAboutData aboutData("Firefox", NULL, ki18n("Firefox KWallet Plugin"), "" );
	KCmdLineArgs::init( &aboutData );
	app = new KApplication(false);
  
	if( checkWallet() )
	  PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::Init() Wallet opened") );
	else
	  PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::Init() Wallet not opened") );
	return NS_OK;
}

NS_IMETHODIMP KDEWallet::InitWithFile(nsIFile *aInputFile,
                                         nsIFile *aOutputFile) {
  PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::InitWithFile() Called") );
    return Init();
}

NS_IMETHODIMP KDEWallet::AddLogin(nsILoginInfo *aLogin) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::AddLogin() Called") );
  
	if( !checkWallet() ) {
		NS_ERROR("Wallet is useless");
		return NS_ERROR_FAILURE;
	}
	nsAutoString s;
	QMap< QString, QString > entry;

	nsAutoString aUsername;
	aLogin->GetUsername(aUsername);
	entry[ kUsernameAttr ] = NS_ConvertUTF16toUTF8(aUsername).get();
	
	aLogin->GetPassword(s);
	entry[ kPasswordAttr ] = NS_ConvertUTF16toUTF8(s).get();
	
	aLogin->GetUsernameField(s);
	entry[ kUsernameFieldAttr ] = NS_ConvertUTF16toUTF8(s).get();
	
	aLogin->GetPasswordField(s);
	entry[ kPasswordFieldAttr ] = NS_ConvertUTF16toUTF8(s).get();
	
	nsAutoString aActionURL;
	aLogin->GetFormSubmitURL(aActionURL);
	if( !aActionURL.IsVoid() )
		entry[ kFormSubmitURLAttr ] = NS_ConvertUTF16toUTF8(aActionURL).get();
	
	nsAutoString aHttpRealm;
	aLogin->GetHttpRealm(aHttpRealm);
	if( !aHttpRealm.IsVoid() )
		entry[ kHttpRealmAttr ] = NS_ConvertUTF16toUTF8(aHttpRealm).get();

	nsAutoString aHostname;
	aLogin->GetHostname(aHostname);
	entry[ kHostnameAttr ] = NS_ConvertUTF16toUTF8(aHostname).get();

	QString key = generateWalletKey( aHostname, aActionURL, aHttpRealm, aUsername );
	if( wallet->writeMap( key, entry ) ) {
		NS_ERROR("Can not save map information");
		return NS_ERROR_FAILURE;
	}
	return NS_OK;
}

NS_IMETHODIMP KDEWallet::RemoveLogin(nsILoginInfo *aLogin) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveLogin() Called") );
  
	if( !checkWallet() ) {
		NS_ERROR("Wallet is useless");
		return NS_ERROR_FAILURE;
	}

	nsAutoString aUsername;
	aLogin->GetUsername(aUsername);
	nsAutoString aActionURL;
	aLogin->GetFormSubmitURL(aActionURL);
	nsAutoString aHttpRealm;
	aLogin->GetHttpRealm(aHttpRealm);
	nsAutoString aHostname;
	aLogin->GetHostname(aHostname);

	QString key = generateWalletKey( aHostname, aActionURL, aHttpRealm, aUsername );
	if( wallet->removeEntry( key ) ) {
		NS_ERROR("Can not remove correctly map information");
		return NS_ERROR_FAILURE;
	}
	return NS_OK;
}

NS_IMETHODIMP KDEWallet::ModifyLogin(nsILoginInfo *oldLogin,
                                        nsISupports *modLogin) {
  /* If the second argument is an nsILoginInfo, 
   * just remove the old login and add the new one */

  PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::ModifyLogin() Called") );
  nsresult interfaceok;
  nsCOMPtr<nsILoginInfo> newLogin( do_QueryInterface(modLogin, &interfaceok) );
  if (interfaceok == NS_OK) {
    nsresult rv = RemoveLogin(oldLogin);
    rv |= AddLogin(newLogin);
  return rv;
  } /* Otherwise, it has to be an nsIPropertyBag.
     * Let's get the attributes from the old login, then append the ones 
     * fetched from the property bag. Gracefully, if an attribute appears
     * twice in an attribut list, the last value is stored. */
  return NS_OK;
}
 

NS_IMETHODIMP KDEWallet::RemoveAllLogins() {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveAllLogins() Called") );
	if( !checkWallet() ) {
		NS_ERROR("Wallet is useless");
		return NS_ERROR_FAILURE;
	}
	
	QString key = generateWalletKey( NS_ConvertUTF8toUTF16( "*" ), NS_ConvertUTF8toUTF16( "*" ), 
					 NS_ConvertUTF8toUTF16( "*" ), NS_ConvertUTF8toUTF16( "*" ) );

	QMap< QString, QMap< QString, QString > > entryMap;
	if( wallet->readMapList( key, entryMap ) != 0 ) 
		return NS_OK;
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveAllLogins() Found %d maps", entryMap.count() ) );
	if( entryMap.count() == 0 ) 
		return NS_OK;

	QMapIterator< QString, QMap< QString, QString > > iterator(entryMap);
	while (iterator.hasNext()) {
		iterator.next();
		if( wallet->removeEntry( iterator.key() ) ) {
			NS_ERROR("Can not remove correctly map information");
			return NS_ERROR_FAILURE;
		}		
	}	
	return NS_OK;
}

NS_IMETHODIMP KDEWallet::FindLogins(PRUint32 *count,
                                       const nsAString & aHostname,
                                       const nsAString & aActionURL,
                                       const nsAString & aHttpRealm,
                                       nsILoginInfo ***logins) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindLogins() Called") );
	*count = 0;
	if( !checkWallet() ) {
		NS_ERROR("Wallet is useless");
		return NS_ERROR_FAILURE;
	}
	
	QString key = generateWalletKey( aHostname, aActionURL, aHttpRealm, NS_ConvertUTF8toUTF16( "*" ) );

	QMap< QString, QMap< QString, QString > > entryMap;
	if( wallet->readMapList( key, entryMap ) != 0 ) 
		return NS_OK;
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindLogins() Found %d maps", entryMap.count() ) );
	if( entryMap.count() == 0 ) 
		return NS_OK;
	nsILoginInfo **array = (nsILoginInfo**) nsMemory::Alloc(entryMap.count() * sizeof(nsILoginInfo*));
	NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);
	memset(array, 0, entryMap.count() * sizeof(nsILoginInfo*));

	QMapIterator< QString, QMap< QString, QString > > iterator(entryMap);
	int i = 0;
	while (iterator.hasNext()) {
		iterator.next();
 		QMap< QString, QString > entry = iterator.value();
		nsCOMPtr<nsILoginInfo> loginInfo = do_CreateInstance(NS_LOGININFO_CONTRACTID);
		if (!loginInfo)
			return NS_ERROR_FAILURE;
		nsAutoString temp;
		if( entry.contains( kHostnameAttr ) )
			loginInfo->SetHostname( QtString2NSString( entry.value( kHostnameAttr ) ) );
		if( entry.contains( kUsernameAttr ) )
			loginInfo->SetUsername(QtString2NSString( entry.value( kUsernameAttr ) ) );
		if( entry.contains( kUsernameFieldAttr ) )
			loginInfo->SetUsernameField(QtString2NSString( entry.value( kUsernameFieldAttr ) ) );
		if( entry.contains( kPasswordAttr ) )
			loginInfo->SetPassword(QtString2NSString( entry.value( kPasswordAttr ) ) );
		if( entry.contains( kPasswordFieldAttr ) )
			loginInfo->SetPasswordField(QtString2NSString( entry.value( kPasswordFieldAttr ) ) );
		if( entry.contains( kFormSubmitURLAttr ) )
			loginInfo->SetFormSubmitURL(QtString2NSString( entry.value( kFormSubmitURLAttr ) ) );
		if( entry.contains( kHttpRealmAttr ) ) 
			loginInfo->SetHttpRealm(QtString2NSString( entry.value( kHttpRealmAttr ) ) );
		NS_ADDREF(loginInfo);
		array[i] = loginInfo;
		i++;
	}	
	*logins = array;
	*count = i;
	return NS_OK;
}

NS_IMETHODIMP KDEWallet::GetAllLogins(PRUint32 *aCount, nsILoginInfo ***aLogins) {
	return FindLogins( aCount, NS_ConvertASCIItoUTF16("*"), NS_ConvertASCIItoUTF16("*"), NS_ConvertASCIItoUTF16("*"), aLogins);
}

NS_IMETHODIMP KDEWallet::SearchLogins(PRUint32 *count,
                                         nsIPropertyBag *matchData,
                                         nsILoginInfo ***logins) {
  PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::SearchLogins() Called") );
  *count = 0;
  return NS_OK;
}

NS_IMETHODIMP KDEWallet::GetAllEncryptedLogins(unsigned int*,
                                                  nsILoginInfo***) {
  PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetAllEncryptedLogins() Called") );
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP KDEWallet::GetAllDisabledHosts(PRUint32 *aCount,
                                                PRUnichar ***aHostnames) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetAllDisabledHosts() Called") );
	*aCount = 0;

	if( !checkWallet() ) {
		NS_ERROR("Wallet is useless");
		return NS_ERROR_FAILURE;
	}
	QMap< QString, QString > saveDisabledHostMap;
	wallet->readMap( kSaveDisabledHostsMapName, saveDisabledHostMap );
	
	if( saveDisabledHostMap.count() == 0 ) 
		return NS_OK;

	PRUnichar **array = (PRUnichar **) nsMemory::Alloc(saveDisabledHostMap.count() * sizeof(PRUnichar *));
	NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);
	memset(array, 0, saveDisabledHostMap.count() * sizeof(PRUnichar*));

	QMapIterator< QString, QString > iterator(saveDisabledHostMap);
	int i = 0;
	while (iterator.hasNext()) {
		iterator.next();
		PRUnichar *nsHostname = NS_StringCloneData( QtString2NSString(iterator.key()) );

		if (!nsHostname)
			return NS_ERROR_FAILURE;
		
		NS_ENSURE_STATE(nsHostname);
		array[i] = nsHostname;
		i++;
	}
	*aCount = i;
	*aHostnames = array;
	return NS_OK;
}

NS_IMETHODIMP KDEWallet::GetLoginSavingEnabled(const nsAString & aHost,
                                                  PRBool *_retval) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetLoginSavingEnabled() Called") );
  
	if( !checkWallet() ) {
		NS_ERROR("Wallet is useless");
		return NS_ERROR_FAILURE;
	}
	QMap< QString, QString > saveDisabledHostMap;

	wallet->readMap( kSaveDisabledHostsMapName, saveDisabledHostMap );

	*_retval = true;
	if( saveDisabledHostMap.contains( NS_ConvertUTF16toUTF8(aHost).get() ) )
		*_retval = false;
	
	if( *_retval )
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetLoginSavingEnabled() saving for %s is enabled", NS_ConvertUTF16toUTF8(aHost).get() ) );
	else
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetLoginSavingEnabled() saving for %s is disabled", NS_ConvertUTF16toUTF8(aHost).get() ) );
	
	return NS_OK;
}

NS_IMETHODIMP KDEWallet::SetLoginSavingEnabled(const nsAString & aHost,
                                                  PRBool isEnabled) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::SetLoginSavingEnabled() Called") );
  
	if( !checkWallet() ) {
		NS_ERROR("Wallet is useless");
		return NS_ERROR_FAILURE;
	}
	QMap< QString, QString > saveDisabledHostMap;

	wallet->readMap( kSaveDisabledHostsMapName, saveDisabledHostMap );
	if( isEnabled ) { //Remove form disabled list, if it is there
		if( saveDisabledHostMap.contains( NS_ConvertUTF16toUTF8(aHost).get() ) )
			if( saveDisabledHostMap.remove( NS_ConvertUTF16toUTF8(aHost).get() ) != 1 ) {
				NS_ERROR("Can not remove save map information");
				return NS_ERROR_FAILURE;
			}
	}
	else 	// Add to disabled list
		saveDisabledHostMap[ NS_ConvertUTF16toUTF8(aHost).get() ] = "TRUE";
	if( wallet->writeMap( kSaveDisabledHostsMapName, saveDisabledHostMap ) ) {
		NS_ERROR("Can not save map information");
		return NS_ERROR_FAILURE;
	}
	return NS_OK;
}

NS_IMETHODIMP KDEWallet::CountLogins(const nsAString & aHostname, 
                                        const nsAString & aActionURL,
                                        const nsAString & aHttpRealm,
                                        PRUint32 *_retval) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountLogins() Called") );
	*_retval = 0;
	if( !checkWallet() ) {
		NS_ERROR("Wallet is useless");
		return NS_ERROR_FAILURE;
	}
	
	QString key = generateWalletKey( aHostname, aActionURL, aHttpRealm, NS_ConvertUTF8toUTF16( "*" ) );

	QMap< QString, QMap< QString, QString > > entryMap;
	if( wallet->readMapList( key, entryMap ) != 0 ) 
		return NS_OK;
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountLogins() Found %d maps", entryMap.count() ) );
	*_retval = entryMap.count();
	return NS_OK;
}


/* End of implementation class template. */

static NS_METHOD
KDEWalletRegisterSelf(nsIComponentManager *compMgr, nsIFile *path,
                         const char *loaderStr, const char *type,
                         const nsModuleComponentInfo *info) {
  nsCOMPtr<nsICategoryManager> cat =
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  NS_ENSURE_STATE(cat);

  cat->AddCategoryEntry("login-manager-storage", "nsILoginManagerStorage",
                        kKDEWalletContractID, PR_TRUE, PR_TRUE, nsnull);
  return NS_OK;
}

static NS_METHOD
KDEWalletUnregisterSelf(nsIComponentManager *compMgr, nsIFile *path,
                           const char *loaderStr,
                           const nsModuleComponentInfo *info ) {
  nsCOMPtr<nsICategoryManager> cat = do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  NS_ENSURE_STATE(cat);

  cat->DeleteCategoryEntry("login-manager-storage", "nsILoginManagerStorage", PR_TRUE);
  return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(KDEWallet)

static const nsModuleComponentInfo components[] = {
  {
    "KDEWallet",
    KDEWALLET_CID,
    kKDEWalletContractID,
    KDEWalletConstructor,
    KDEWalletRegisterSelf,
    KDEWalletUnregisterSelf
  }
};

NS_IMPL_NSGETMODULE(KDEWallet, components)
 
