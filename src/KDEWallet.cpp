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
/* Include the requried KDE headers. */
#include <kapplication.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <kwallet.h>

#include "KDEWallet.h"

#include "nsILoginInfo.h"
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
#include "mozilla/ModuleUtils.h"

extern PRLogModuleInfo *gKDEWalletLog;

NS_GENERIC_FACTORY_CONSTRUCTOR(KDEWallet)
NS_DEFINE_NAMED_CID(KDEWALLET_CID);

static const mozilla::Module::CIDEntry kKDEWalletCIDs[] = {
    { &kKDEWALLET_CID, false, NULL, KDEWalletConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kKDEWalletContracts[] = {
    { KDEWALLET_CONTRACTID, &kKDEWALLET_CID },
    { NULL }
};

static const mozilla::Module::CategoryEntry kKDEWalletCategories[] = {
    { "login-manager-storage", "nsILoginManagerStorage", KDEWALLET_CONTRACTID },
    { NULL }
};

static const mozilla::Module kKDEWalletModule = {
    mozilla::Module::kVersion,
    kKDEWalletCIDs,
    kKDEWalletContracts,
    kKDEWalletCategories
};

NSMODULE_DEFN(nsKDEWalletModule) = &kKDEWalletModule;
PRLogModuleInfo *gKDEWalletLog;

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

NS_IMETHODIMP GetPreference( const char *which, QString &preference ) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetPreference() Called") );
	nsresult res;

	nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &res);
	NS_ENSURE_SUCCESS(res, res);
	if( prefs == nsnull )
		return NS_ERROR_FAILURE;

	nsCOMPtr<nsIPrefBranch> branch;
	res = prefs->GetBranch("extensions.firefox-kde-wallet.", getter_AddRefs(branch));
	NS_ENSURE_SUCCESS(res, res);
	if( branch == nsnull )
		return NS_ERROR_FAILURE;

	char* value = NULL;
	branch->GetCharPref( which, &value);
	if (value == NULL)
		return NS_ERROR_FAILURE;

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetPreference() %s=%s", which, value) );
	preference = QString( value );
	return NS_OK;
}

NS_IMETHODIMP checkWallet( void ) {
	nsresult res;
	
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::checkWallet() Called" ) );
	if( !wallet ) {
		QString walletPref;
		res = GetPreference( "wallet", walletPref );	
		NS_ENSURE_SUCCESS(res, res);
	  
		if( walletPref == "LocalWallet" )
			wallet =  KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous );
		if( walletPref == "NetworkWallet" )
			wallet =  KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), 0, KWallet::Wallet::Synchronous );
		if( !wallet ) {
			PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::checkWallet() Could not open %s", walletPref.toUtf8().data() ) );
			return NS_ERROR_FAILURE;
		}
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::checkWallet() %s Opened", walletPref.toUtf8().data() ) );  
	}
	
	QString folderPref;
	res = GetPreference( "folder", folderPref );	
	NS_ENSURE_SUCCESS(res, res);
	if( !wallet->hasFolder( folderPref ) ) {
		if( !wallet->createFolder( folderPref ) ) {
			PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::checkWallet() Could not create folder %s", folderPref.toUtf8().data() ) );
			return NS_ERROR_FAILURE;
		}
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::checkWallet() Folder %s created", folderPref.toUtf8().data() ) );  
	}
	if( !wallet->setFolder( folderPref ) ) {
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::checkWallet() Could not select folder %s", folderPref.toUtf8().data() ) );
		return NS_ERROR_FAILURE;
	}
	
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::checkWallet() Folder %s selected", folderPref.toUtf8().data() ) );  
	return NS_OK;
}

nsAutoString QtString2NSString ( const QString &qtString ) {
	return NS_ConvertUTF8toUTF16( qtString.toUtf8().data() );
}

QString NSString2QtString ( const nsAString & nsas ) {
	nsAutoString nsautos(nsas);
	return QString::fromUtf16(nsautos.get());
}

QString generateWalletKey( const nsAString & aHostname,
			const nsAString & aActionURL,
			const nsAString & aHttpRealm,
			const nsAString & aUsername ) {
	QString key = (aUsername.IsVoid() || aUsername.IsEmpty() ) ? "" : NSString2QtString(aUsername);
	key += ",";
	key += (aActionURL.IsVoid() || aActionURL.IsEmpty() ) ? "" : NSString2QtString(aActionURL);
	key += ",";
	key += (aHttpRealm.IsVoid() || aHttpRealm.IsEmpty() ) ? "" : NSString2QtString(aHttpRealm);
	key += ",";
	key += (aHostname.IsVoid() || aHostname.IsEmpty() ) ? "" : NSString2QtString(aHostname);
	return key;
}

QString generateQueryWalletKey( const nsAString & aHostname,
			const nsAString & aActionURL,
			const nsAString & aHttpRealm,
			const nsAString & aUsername ) {
	QString key = (aUsername.IsVoid() || aUsername.IsEmpty() ) ? "*" : NSString2QtString(aUsername);
	key += ",";
	key += (aActionURL.IsVoid() || aActionURL.IsEmpty() ) ? "*" : NSString2QtString(aActionURL);
	key += ",";
	key += (aHttpRealm.IsVoid() || aHttpRealm.IsEmpty() ) ? "*" : NSString2QtString(aHttpRealm);
	key += ",";
	key += (aHostname.IsVoid() || aHostname.IsEmpty() ) ? "*" : NSString2QtString(aHostname);
	return key;
}

KDEWallet::KDEWallet() {
	gKDEWalletLog = PR_NewLogModule("KDEWalletLog");
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::KDEWallet() Called") );
}

KDEWallet::~KDEWallet() {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::~KDEWallet() Called") );
}

NS_IMETHODIMP KDEWallet::InitWithFile(nsIFile *aInputFile,
                                         nsIFile *aOutputFile) {
     PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::InitWithFile() Called") );
     return Init();
}

NS_IMETHODIMP KDEWallet::GetUiBusy(int *) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetUiBusy() Called") );
	return NS_OK;
}

NS_IMETHODIMP KDEWallet::Init() {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::Init() Called") );
  
	/* KWallet requries a functioning KApplication or it will segfault */
	KAboutData aboutData("Firefox", NULL, ki18n("Firefox KWallet Plugin"), "" );
	KCmdLineArgs::init( &aboutData );
	app = new KApplication(false);
  
	nsresult res = checkWallet();
	NS_ENSURE_SUCCESS(res, res);

	return NS_OK;
}


NS_IMETHODIMP AddLoginWithPassword(nsILoginInfo *aLogin, const QString &password = 0 ) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "AddLoginWithPassword() Called") );
  
	nsresult res = checkWallet();
	NS_ENSURE_SUCCESS(res, res);

	nsAutoString s;
	QMap< QString, QString > entry;

	nsAutoString aUsername;
	aLogin->GetUsername(aUsername);
	entry[ kUsernameAttr ] = NSString2QtString(aUsername);
	
	if( password.isEmpty() ) {
		aLogin->GetPassword(s);
		entry[ kPasswordAttr ] = NSString2QtString(s);
	}
	else
		entry[ kPasswordAttr ] = password;

	
	aLogin->GetUsernameField(s);
	entry[ kUsernameFieldAttr ] = NSString2QtString(s);
	
	aLogin->GetPasswordField(s);
	entry[ kPasswordFieldAttr ] = NSString2QtString(s);
	
	nsAutoString aActionURL;
	aLogin->GetFormSubmitURL(aActionURL);
	if( !aActionURL.IsVoid() )
		entry[ kFormSubmitURLAttr ] = NSString2QtString(aActionURL);
	
	nsAutoString aHttpRealm;
	aLogin->GetHttpRealm(aHttpRealm);
	if( !aHttpRealm.IsVoid() )
		entry[ kHttpRealmAttr ] = NSString2QtString(aHttpRealm);

	nsAutoString aHostname;
	aLogin->GetHostname(aHostname);
	entry[ kHostnameAttr ] = NSString2QtString(aHostname);

	QString key = generateWalletKey( aHostname, aActionURL, aHttpRealm, aUsername );
	if( wallet->writeMap( key, entry ) ) {
		NS_ERROR("Can not save map information");
		return NS_ERROR_FAILURE;
	}
	return NS_OK;
}


NS_IMETHODIMP KDEWallet::AddLogin(nsILoginInfo *aLogin ) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::AddLogin() Called") );
  
	return AddLoginWithPassword( aLogin );
}

NS_IMETHODIMP KDEWallet::RemoveLogin(nsILoginInfo *aLogin) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveLogin() Called") );
  
	nsresult res = checkWallet();
	NS_ENSURE_SUCCESS(res, res);

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
	nsCOMPtr<nsIPropertyBag> propBag( do_QueryInterface(modLogin, &interfaceok) );
	if (interfaceok == NS_OK) {
		nsCOMPtr<nsISimpleEnumerator> enumerator;
		nsresult rv = propBag->GetEnumerator(getter_AddRefs(enumerator));
		NS_ENSURE_SUCCESS(rv, rv);

		nsCOMPtr<nsISupports> sup;
		nsCOMPtr<nsIProperty> prop;
		nsAutoString propName;
		PRBool hasMoreElements;
		
		rv = enumerator->HasMoreElements(&hasMoreElements);
		NS_ENSURE_SUCCESS(rv, rv);

		while( hasMoreElements ) {
			rv = enumerator->GetNext(getter_AddRefs(sup));                        
			NS_ENSURE_SUCCESS(rv, rv);

			rv = enumerator->HasMoreElements(&hasMoreElements);
			NS_ENSURE_SUCCESS(rv, rv);
			
			prop = do_QueryInterface(sup, &rv);                                   
			NS_ENSURE_SUCCESS(rv, rv);
											      
			prop->GetName(propName);   

			if( propName.EqualsLiteral("password") ) {
				nsCOMPtr<nsIVariant> val;
				PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::ModifyLogin() change password" ) );
				prop->GetValue(getter_AddRefs(val));                                  
				if (!val)                                                           
					return NS_ERROR_FAILURE;  
				nsString valueString;
				
				rv = val->GetAsDOMString(valueString);
				NS_ENSURE_SUCCESS(rv, rv);
				PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::ModifyLogin() password %s", NS_ConvertUTF16toUTF8(valueString).get() ) );
				return AddLoginWithPassword( oldLogin, NSString2QtString( valueString ) );
			}
		}
		return NS_OK;
	}
	NS_ERROR("Dont know how to modify a login with modLogin");
	return NS_ERROR_FAILURE;  
}
 

NS_IMETHODIMP KDEWallet::RemoveAllLogins() {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveAllLogins() Called") );

	nsresult res = checkWallet();
	NS_ENSURE_SUCCESS(res, res);
	
	QString key = generateQueryWalletKey( NS_ConvertUTF8toUTF16( "*" ), NS_ConvertUTF8toUTF16( "*" ), 
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

	nsresult res = checkWallet();
	NS_ENSURE_SUCCESS(res, res);
	
	QString key = generateQueryWalletKey( aHostname, aActionURL, aHttpRealm, NS_ConvertUTF8toUTF16( "*" ) );

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
		NS_ADDREF(loginInfo);
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
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindLogins() Found key: %s", iterator.key().toUtf8().data() ) );
		array[i] = loginInfo;
		i++;
	}	
	*logins = array;
	*count = i;
	return NS_OK;
}

NS_IMETHODIMP KDEWallet::GetAllLogins(PRUint32 *aCount, nsILoginInfo ***aLogins) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetAllLogins() Called") );
	return FindLogins( aCount, NS_ConvertASCIItoUTF16("*"), NS_ConvertASCIItoUTF16("*"), NS_ConvertASCIItoUTF16("*"), aLogins);
}

NS_IMETHODIMP KDEWallet::SearchLogins(PRUint32 *aCount,
                                         nsIPropertyBag *aMatchData,
                                         nsILoginInfo ***aLogins) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::SearchLogins() Called") );
	nsCOMPtr<nsISimpleEnumerator> enumerator;
	nsresult rv = aMatchData->GetEnumerator(getter_AddRefs(enumerator));
	NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsISupports> sup;
	nsCOMPtr<nsIProperty> prop;
	nsAutoString propName;
	PRBool hasMoreElements;
	
	rv = enumerator->HasMoreElements(&hasMoreElements);
	NS_ENSURE_SUCCESS(rv, rv);

	while( hasMoreElements ) {
		rv = enumerator->GetNext(getter_AddRefs(aMatchData));                        
		NS_ENSURE_SUCCESS(rv, rv);

		rv = enumerator->HasMoreElements(&hasMoreElements);
		NS_ENSURE_SUCCESS(rv, rv);
		
		prop = do_QueryInterface(sup, &rv);                                   
		NS_ENSURE_SUCCESS(rv, rv);
										      
		prop->GetName(propName);   
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::SearchLogins() property %s", NS_ConvertUTF16toUTF8(propName).get() ) );
		nsCOMPtr<nsIVariant> val;
		prop->GetValue(getter_AddRefs(val));                                  
		if (val) {
			nsString valueString;
			rv = val->GetAsDOMString(valueString);
			NS_ENSURE_SUCCESS(rv, rv);
			PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::SearchLogins() value %s", NS_ConvertUTF16toUTF8(valueString).get() ) );  
		}
/*
		if( propName.EqualsLiteral("password") ) {
			nsCOMPtr<nsIVariant> val;
			PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::ModifyLogin() change password" ) );
			prop->GetValue(getter_AddRefs(val));                                  
			if (!val)                                                           
				return NS_ERROR_FAILURE;  
			nsString valueString;
			
			rv = val->GetAsDOMString(valueString);
			NS_ENSURE_SUCCESS(rv, rv);
			PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::ModifyLogin() password %s", NS_ConvertUTF16toUTF8(valueString).get() ) );
			return AddLoginWithPassword( oldLogin, NSString2QtString( valueString ) );
		}
*/	}
	return FindLogins( aCount, NS_ConvertASCIItoUTF16("*"), NS_ConvertASCIItoUTF16("*"), NS_ConvertASCIItoUTF16("*"), aLogins);
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

	nsresult res = checkWallet();
	NS_ENSURE_SUCCESS(res, res);

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
  
	nsresult res = checkWallet();
	NS_ENSURE_SUCCESS(res, res);

	QMap< QString, QString > saveDisabledHostMap;

	wallet->readMap( kSaveDisabledHostsMapName, saveDisabledHostMap );

	*_retval = true;
	if( saveDisabledHostMap.contains(  NSString2QtString(aHost) ) )
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
  
	nsresult res = checkWallet();
	NS_ENSURE_SUCCESS(res, res);

	QMap< QString, QString > saveDisabledHostMap;

	wallet->readMap( kSaveDisabledHostsMapName, saveDisabledHostMap );
	if( isEnabled ) { //Remove form disabled list, if it is there
		if( saveDisabledHostMap.contains( NSString2QtString(aHost) ) )
			if( saveDisabledHostMap.remove( NSString2QtString(aHost) ) != 1 ) {
				NS_ERROR("Can not remove save map information");
				return NS_ERROR_FAILURE;
			}
	}
	else 	// Add to disabled list
		saveDisabledHostMap[ NSString2QtString(aHost) ] = "TRUE";
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

	nsresult res = checkWallet();
	NS_ENSURE_SUCCESS(res, res);
	
	QString key = generateQueryWalletKey( aHostname, aActionURL, aHttpRealm, NS_ConvertUTF8toUTF16( "*" ) );

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountLogins() key: %s", key.toUtf8().data() ) );
	QMap< QString, QMap< QString, QString > > entryMap;
	if( wallet->readMapList( key, entryMap ) != 0 ) 
		return NS_OK;
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountLogins() Found %d maps", entryMap.count() ) );
	*_retval = entryMap.count();
	return NS_OK;
}
