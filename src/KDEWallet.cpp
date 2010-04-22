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
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMElement.h"
#include "nsIWindowMediator.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMWindow.h"

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

nsAutoString QtString2NSString ( const QString &qtString ) {
	return NS_ConvertUTF8toUTF16( qtString.toUtf8().data() );
}

QString NSString2QtString ( const nsAString & nsas ) {
	nsAutoString nsautos(nsas);
	return QString::fromUtf16(nsautos.get());
}

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

NS_IMETHODIMP GetFolderPreference( QString &preference ) {
	return GetPreference( "folder", preference );
}

NS_IMETHODIMP GetWalletPreference( QString &preference ) {
	return GetPreference( "wallet", preference );
}

NS_IMETHODIMP checkWallet( void ) {
	QString walletPref;
	nsresult res = GetWalletPreference( walletPref );	
	NS_ENSURE_SUCCESS(res, res);
		
	if( !wallet ) {
		if( walletPref == "LocalWallet" )
			wallet =  KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous );
		if( walletPref == "NetworkWallet" )
			wallet =  KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), 0, KWallet::Wallet::Synchronous );
		if( !wallet )
			return NS_ERROR_FAILURE;
	}
	return NS_OK;
}

NS_IMETHODIMP selectFolder( const QString &folder ) {
	nsresult res = checkWallet();
	NS_ENSURE_SUCCESS(res, res);

	if( !wallet->hasFolder( folder ) ) {
		if( !wallet->createFolder( folder ) )
			return NS_ERROR_FAILURE;
	}
	if( !wallet->setFolder( folder ) )
		return NS_ERROR_FAILURE;
	return NS_OK;
}

NS_IMETHODIMP selectPasswordFolder( void ) {
	return selectFolder( KWallet::Wallet::PasswordFolder() );
}

NS_IMETHODIMP selectFormDataFolder( void ) {
	return selectFolder( KWallet::Wallet::FormDataFolder() );
}

NS_IMETHODIMP selectDefaultFolder( void ) {
	QString folderPref;
	nsresult res = GetFolderPreference( folderPref );
	NS_ENSURE_SUCCESS(res, res);
	return selectFolder( folderPref );
}

NS_IMETHODIMP generateRealmWalletKey(  const nsAString & aHostname,
			const nsAString & aHttpRealm,
			QString & key) {
	if( aHostname.IsVoid() || aHostname.IsEmpty() )
		return NS_ERROR_FAILURE;

	key = NSString2QtString(aHostname);

	key.replace( "://", "-" );

	if( !key.contains(":") )
		key += ":-1"; //I think this is a bug, it should be :80 or whatever

	if( aHttpRealm.IsVoid() )
		return NS_ERROR_FAILURE;

	if( key.startsWith( "http" ) ) {
		key += "-";
		key += NSString2QtString(aHttpRealm);
	}

	return NS_OK;
}

NS_IMETHODIMP generateRealmWalletSearchKey(  const nsAString & aHostname,
			const nsAString & aHttpRealm,
			QString & key) {
	if( aHostname.IsVoid() || aHostname.IsEmpty() )
		return NS_ERROR_FAILURE;

	key = NSString2QtString(aHostname);

	key.replace( "://", "-*" ); //URL may contain username, as ftp://guille@somehost.com/

	if( !key.contains(":") )
		key += ":*"; //I think this is a bug, it should be :80 or whatever

	if( aHttpRealm.IsVoid() )
		return NS_ERROR_FAILURE;

	if( key.startsWith( "http" ) ) {
		key += "-";
		key += NSString2QtString(aHttpRealm);
	}

	return NS_OK;
}

QString generateBulkWalletKey( const nsAString & aHostname,
			const nsAString & aActionURL,
			const nsAString & aHttpRealm,
			const nsAString & aUsername ) {
	QString key = (aUsername.IsVoid() || aUsername.IsEmpty() ) ? "" : NSString2QtString(aUsername).replace(' ', "%20");
	key += ",";
	key += (aActionURL.IsVoid() || aActionURL.IsEmpty() ) ? "" : NSString2QtString(aActionURL);
	key += ",";
	key += (aHttpRealm.IsVoid() || aHttpRealm.IsEmpty() ) ? "" : NSString2QtString(aHttpRealm);
	key += ",";
	key += (aHostname.IsVoid() || aHostname.IsEmpty() ) ? "" : NSString2QtString(aHostname);
	return key;
}

QString generateBulkWalletSearchKey( const nsAString & aHostname,
			const nsAString & aActionURL,
			const nsAString & aHttpRealm,
			const nsAString & aUsername ) {
	QString key = (aUsername.IsVoid() || aUsername.IsEmpty() ) ? "*" : NSString2QtString(aUsername).replace(' ', "%20");
	key += ",";
	key += (aActionURL.IsVoid() || aActionURL.IsEmpty() ) ? "*" : NSString2QtString(aActionURL);
	key += ",";
	key += (aHttpRealm.IsVoid() || aHttpRealm.IsEmpty() ) ? "*" : NSString2QtString(aHttpRealm);
	key += ",";
	key += (aHostname.IsVoid() || aHostname.IsEmpty() ) ? "*" : NSString2QtString(aHostname);
	return key;
}

NS_IMETHODIMP GetDocument( nsIDOMDocument **document ) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetDocument() Called") );
	nsresult res;

	nsCOMPtr<nsIWindowMediator> mediator = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &res);
	NS_ENSURE_SUCCESS(res, res);
	if( mediator == nsnull )
		return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMWindowInternal> window;
	res = mediator->GetMostRecentWindow(nsnull, getter_AddRefs(window) );
	NS_ENSURE_SUCCESS(res, res);
	if( window == nsnull )
		return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMWindow> content;
	res = window->GetContent( getter_AddRefs(content) );
	NS_ENSURE_SUCCESS(res, res);
	if( content == nsnull )
		return NS_ERROR_FAILURE;

	res = content->GetDocument( document );
	NS_ENSURE_SUCCESS(res, res);
	if( document == nsnull )
		return NS_ERROR_FAILURE;
	
	return NS_OK;
}

NS_IMETHODIMP GetURL( QString & qsurl ) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetURL() Called") );
	nsresult res;

	nsCOMPtr<nsIDOMDocument> document;
	res = GetDocument( getter_AddRefs(document) );
	NS_ENSURE_SUCCESS(res, res);

	nsCOMPtr<nsIDOMHTMLDocument> html_document( do_QueryInterface(document, &res ) );
	NS_ENSURE_SUCCESS(res, res);
	if( html_document == nsnull )
		return NS_ERROR_FAILURE;

	nsString url;
	res = html_document->GetURL(url);
	NS_ENSURE_SUCCESS(res, res);

	qsurl = NSString2QtString( url );
	
	qsurl = qsurl.section( '?', 0, 0 ).section( '#', 0, 0 ).section( ';', 0, 0 );
	
	return NS_OK;
}  

NS_IMETHODIMP GetFormsAndPasswordsNames( QStringList & formNames, QStringList & passwordNames ) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetFormsAndPasswordsNames() Called") );
	nsresult res;

	nsCOMPtr<nsIDOMDocument> document;
	res = GetDocument( getter_AddRefs(document) );
	NS_ENSURE_SUCCESS(res, res);

	nsCOMPtr<nsIDOMNodeList> contextList;
	res = document->GetElementsByTagName( NS_LITERAL_STRING("form"), getter_AddRefs(contextList) );
	NS_ENSURE_SUCCESS(res, res);
	if( contextList == nsnull )
		return NS_ERROR_FAILURE;

	PRUint32 length;
	res = contextList->GetLength(&length);
	NS_ENSURE_SUCCESS(res, res);

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetFormsAndPasswordsNames() Found %d forms", length) );

	PRUint32 index;
	// the name of a context is in its "name" attribute
	NS_NAMED_LITERAL_STRING( nameAttr, "name" );
	for (index = 0; index < length; index++)
	{
		nsCOMPtr<nsIDOMNode> node;
		res = contextList->Item(index, getter_AddRefs(node));
		NS_ENSURE_SUCCESS(res, res);
		
		nsAutoString formName;
		nsCOMPtr<nsIDOMElement> form = do_QueryInterface(node);
		res = form->GetAttribute(nameAttr, formName);
		NS_ENSURE_SUCCESS(res, res);

		nsCOMPtr<nsIDOMNodeList> formContextList;
		res = form->GetElementsByTagName( NS_LITERAL_STRING("input"), getter_AddRefs(formContextList) );
		NS_ENSURE_SUCCESS(res, res);

		PRUint32 formLength;
		res = formContextList->GetLength(&formLength);
		NS_ENSURE_SUCCESS(res, res);

		PRUint32 formIndex;
		for (formIndex = 0; formIndex < formLength; formIndex++)
		{
			nsCOMPtr<nsIDOMNode> formNode;
			res = formContextList->Item(formIndex, getter_AddRefs(formNode));
			NS_ENSURE_SUCCESS(res, res);

			nsCOMPtr<nsIDOMElement> formElt = do_QueryInterface(formNode);

			nsAutoString type;
			res = formElt->GetAttribute(NS_LITERAL_STRING("type"), type);
			NS_ENSURE_SUCCESS(res, res);

			if( type == NS_LITERAL_STRING("password") ) {
				formNames += NSString2QtString( formName );
				PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetFormsAndPasswordsNames() form name: %s",  NS_ConvertUTF16toUTF8(formName).get() ) );
				
				nsAutoString passwordName;
				res = formElt->GetAttribute(nameAttr, passwordName);
				NS_ENSURE_SUCCESS(res, res);
				
				PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetFormsAndPasswordsNames() password element name: %s",  NS_ConvertUTF16toUTF8(passwordName).get() ) );
				passwordNames += NSString2QtString( passwordName );
			}
		}
	}
	return NS_OK;
}

NS_IMETHODIMP KDEWallet::Init() {
	gKDEWalletLog = PR_NewLogModule("KDEWalletLog");

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::Init() Called") );
  
	/* KWallet requries a functioning KApplication or it will segfault */
	KAboutData aboutData("Firefox", NULL, ki18n("Firefox KWallet Plugin"), "" );
	KCmdLineArgs::init( &aboutData );
	app = new KApplication(false);

	return checkWallet();
}

NS_IMETHODIMP KDEWallet::InitWithFile(nsIFile *aInputFile,
                                         nsIFile *aOutputFile) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::InitWithFile() Called") );
	return Init();
}

NS_IMETHODIMP RemoveRealmLogin(nsILoginInfo *aLogin) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveRealmLogin() Called") );

	nsAutoString aHostname;
	aLogin->GetHostname(aHostname);
	nsAutoString aHttpRealm;
	aLogin->GetHttpRealm(aHttpRealm);

	QString key;

	nsresult res = generateRealmWalletKey( aHostname, aHttpRealm, key );
	NS_ENSURE_SUCCESS(res, res);

	nsAutoString temp = QtString2NSString( key );
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveRealmLogin() search key: %s", NS_ConvertUTF16toUTF8(temp).get() ) );

	res = selectPasswordFolder();
	NS_ENSURE_SUCCESS(res, res);

	QMap< QString, QMap< QString, QString > > entryMap;
	if( wallet->readMapList( key, entryMap ) != 0 )
		return NS_OK;
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveRealmLogin() Found %d maps", entryMap.count() ) );
	if( entryMap.count() == 0 )
		return NS_OK;

	QMapIterator< QString, QMap< QString, QString > > iterator(entryMap);

	nsAutoString aUsername;
	aLogin->GetUsername(aUsername);
	QString username =  NSString2QtString(aUsername);
	nsAutoString aPassword;
	aLogin->GetPassword(aPassword);
	QString password = NSString2QtString(aPassword);

	while (iterator.hasNext()) {
		iterator.next();

		temp = QtString2NSString( iterator.key() );
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveRealmLogin() key: %s", NS_ConvertUTF16toUTF8(temp).get() ) );

 		QMap< QString, QString > entry = iterator.value();

		QString usernameField = entry.key( username );
		if( !usernameField.isEmpty() ) {
			temp = QtString2NSString( usernameField );
			PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveRealmLogin() login key: %s", NS_ConvertUTF16toUTF8(temp).get() ) );

			QString passwordField = "password";
			if( usernameField.startsWith("login") && usernameField.size() > 5 )
				passwordField += usernameField.mid(5); // eg: password-2, password-3, etc

			temp = QtString2NSString( passwordField );
			PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveRealmLogin() password key: %s", NS_ConvertUTF16toUTF8(temp).get() ) );

			if( entry.value( passwordField ) == password ) {
				entry.remove( usernameField );
				entry.remove( passwordField );
				if( entry.isEmpty() ) {
					if( wallet->removeEntry( iterator.key() ) ) {
						NS_ERROR("Can not remove correctly map information");
						return NS_ERROR_FAILURE;
					}
				}
				else {
					if( wallet->writeMap( iterator.key(), entry ) ) {
						NS_ERROR("Can not save map information");
						return NS_ERROR_FAILURE;
					}
				}
				return NS_OK;
			}
		}
	}

	return NS_OK;
}

NS_IMETHODIMP RemoveBulkLogin(nsILoginInfo *);

NS_IMETHODIMP RemoveFormLogin(nsILoginInfo *aLogin) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() Called") );
	//First check if it is in the bulk store:
	nsresult res = RemoveBulkLogin(aLogin);
	if( res == NS_OK ) {
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() Removed bulk key" ) );
		return res;
	}
	
	NS_ENSURE_SUCCESS(res, res);
	
	nsAutoString aHostname;
	aLogin->GetHostname(aHostname);
	QString key = NSString2QtString(aHostname) + "*#*";

	QString privateKey = key + " *";

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() Trying to remove from private store" ) );

	// if we are lucky, password is stored in private store
	res = selectDefaultFolder();
	NS_ENSURE_SUCCESS(res, res);

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() search key: %s", privateKey.toUtf8().data() ) );
	QMap< QString, QMap< QString, QString > > entryMap;
	if( wallet->readMapList( key, entryMap ) != 0 )
		return NS_ERROR_FAILURE;
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() Found %d Private logins", entryMap.count() ) );

	nsAutoString aFieldInfo;
	aLogin->GetUsername(aFieldInfo);
	QString fieldInfo =  NSString2QtString(aFieldInfo);
	QStringList field = fieldInfo.split( "|" );
	nsAutoString aDataInfo;
	aLogin->GetPassword(aDataInfo);
	QString dataInfo =  NSString2QtString(aDataInfo);
	QStringList data = dataInfo.split( "|" );

	QMapIterator< QString, QMap< QString, QString > > iterator(entryMap);
	while (iterator.hasNext()) {
		iterator.next();
		QString actualKey = iterator.key();
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() private key %s", actualKey.toUtf8().data() ) );

 		QMap< QString, QString > entry = iterator.value();
		if( entry.contains( field[0] ) && entry.contains( field[1] ) ) {
			if( entry[ field[0] ] == data[0] && entry[ field[1] ] == data[1] ) { // We've got a coincidence!!
				if( wallet->removeEntry( actualKey ) ) {
					NS_ERROR("Can not remove map information correctly");
					return NS_ERROR_FAILURE;
				}
				PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() Removed private key: %s", actualKey.toUtf8().data() ) );
				return NS_OK;
			}
		}
	}

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() Trying to remove from Form Data store" ) );

	// password must be in Form Data, we have to remove it, and move one login info into Form Data
	res = selectFormDataFolder();
	NS_ENSURE_SUCCESS(res, res);

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() Form Data search key: %s", key.toUtf8().data() ) );
	QMap< QString, QMap< QString, QString > > entryFDMap;
	if( wallet->readMapList( key, entryFDMap ) != 0 )
		return NS_ERROR_FAILURE;
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() Found %d Form Data logins", entryFDMap.count() ) );

	QString actualKey;
	QMapIterator< QString, QMap< QString, QString > > iterator2(entryFDMap);
	while (iterator2.hasNext()) {
		iterator2.next();
		actualKey = iterator2.key();
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() Form Data key %s", actualKey.toUtf8().data() ) );

 		QMap< QString, QString > entry = iterator2.value();
		if( entry.contains( field[0] ) && entry.contains( field[1] ) ) {
			if( entry[ field[0] ] == data[0] && entry[ field[1] ] == data[1] ) { // We've got a coincidence!!
				if( wallet->removeEntry( actualKey ) ) {
					NS_ERROR("Can not remove map information correctly");
					return NS_ERROR_FAILURE;
				}
				PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() Removed Form Data key: %s", actualKey.toUtf8().data() ) );
				break;
			}
		}
	}

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() Removed from Form Data store, try to move an entry from private store" ) );

	QString searchKey = actualKey + " *";
	res = selectDefaultFolder();
	NS_ENSURE_SUCCESS(res, res);

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() search private key: %s", searchKey.toUtf8().data() ) );
	QMap< QString, QMap< QString, QString > > privateMap;
	if( wallet->readMapList( searchKey, privateMap ) != 0 )
		return NS_ERROR_FAILURE;
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() Found %d Private logins", privateMap.count() ) );

	if( privateMap.count() == 0 ) // we have nothing to move
		return NS_OK;

	QMapIterator< QString, QMap< QString, QString > > privateIterator(privateMap);

	while (privateIterator.hasNext()) {
		privateIterator.next();
		QMap< QString, QString > loginMap = privateIterator.value();

		if( wallet->removeEntry( privateIterator.key() ) ) {
			NS_ERROR("Can not remove map information correctly");
			return NS_ERROR_FAILURE;
		}
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() Removed private key: %s", privateIterator.key().toUtf8().data() ) );

		res = selectFormDataFolder();
		NS_ENSURE_SUCCESS(res, res);

		if( wallet->writeMap( actualKey, loginMap ) ) {
			NS_ERROR("Can not save map information");
			return NS_ERROR_FAILURE;
		}

		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveFormLogin() Added Form Data key %s", actualKey.toUtf8().data() ) );
		return NS_OK;
	}

	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP RemoveBulkLogin(nsILoginInfo *aLogin) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveBulkLogin() Called") );

	nsresult res = selectDefaultFolder();
	NS_ENSURE_SUCCESS(res, res);

	nsAutoString aUsername;
	aLogin->GetUsername(aUsername);
	nsAutoString aActionURL;
	aLogin->GetFormSubmitURL(aActionURL);
	nsAutoString aHttpRealm;
	aLogin->GetHttpRealm(aHttpRealm);
	nsAutoString aHostname;
	aLogin->GetHostname(aHostname);

	QString key = generateBulkWalletKey( aHostname, aActionURL, aHttpRealm, aUsername );
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveBulkLogin() bulk key %s", key.toUtf8().data() ) );
	if( wallet->hasEntry( key ) ) {
		if( wallet->removeEntry( key ) ) {
			NS_ERROR("Can not remove correctly map information");
			return NS_ERROR_FAILURE;
		}
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveBulkLogin() removed bulk key %s", key.toUtf8().data() ) );
		return NS_OK;
	}
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP KDEWallet::RemoveLogin(nsILoginInfo *aLogin) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveLogin() Called") );

	nsAutoString aActionURL;
	aLogin->GetFormSubmitURL(aActionURL);
	if( aActionURL.IsVoid() )
		return RemoveRealmLogin( aLogin );

	nsAutoString aHttpRealm;
	aLogin->GetHttpRealm(aHttpRealm);
	if( aHttpRealm.IsVoid() )
		return RemoveFormLogin( aLogin );

	NS_ERROR("RemoveLogin must set aActionURL or aHttpRealm");
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP CountRealmLogins(const nsAString &, const nsAString &, PRUint32 *);

NS_IMETHODIMP FindRealmLogins(PRUint32 *count,
				const nsAString & aHostname,
				const nsAString & aHttpRealm,
				nsILoginInfo ***logins) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindRealmLogins() Called") );

	nsresult res = CountRealmLogins( aHostname, aHttpRealm, count );
	NS_ENSURE_SUCCESS(res, res);

	if( *count == 0 )
		return NS_OK; //There are no logins, good bye

	nsILoginInfo **array = (nsILoginInfo**) nsMemory::Alloc( *count * sizeof(nsILoginInfo*));
	NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);
	memset(array, 0, *count * sizeof(nsILoginInfo*));

	QString key;

	res = generateRealmWalletSearchKey( aHostname, aHttpRealm, key );
	NS_ENSURE_SUCCESS(res, res);

	nsAutoString temp = QtString2NSString( key );
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindRealmLogins() search key: %s", NS_ConvertUTF16toUTF8(temp).get() ) );

	QMap< QString, QMap< QString, QString > > entryMap;
	if( wallet->readMapList( key, entryMap ) != 0 )
		return NS_OK;
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindRealmLogins() Found %d maps", entryMap.count() ) );

	QMapIterator< QString, QMap< QString, QString > > iterator(entryMap);
	int i = 0;
	while (iterator.hasNext()) {
		iterator.next();

		temp = QtString2NSString( iterator.key() );
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindRealmLogins() key: %s", NS_ConvertUTF16toUTF8(temp).get() ) );

		QStringList keyParts = iterator.key().split( "-" );
		QString hostname = keyParts[0] + "://" + keyParts[1];
		QString httpRealm;
		int first = 2;
		if( hostname.endsWith( ":" ) ) { // http-something:-1-Realm
			hostname.truncate( hostname.size()-1 );
			first = 3;
		}

		if( hostname.startsWith( "http" ) ) {
			httpRealm = keyParts[ first ];
			for( int i = first + 1; i < keyParts.size(); i++ ) {
				httpRealm += "-";
				httpRealm += keyParts[ i ];
			}
		}

		res = selectPasswordFolder();
		NS_ENSURE_SUCCESS(res, res);

 		QMap< QString, QString > entry = iterator.value();
		QMapIterator< QString, QString > entriesIterator(entry);
		while (entriesIterator.hasNext()) {
			entriesIterator.next();
			if( entriesIterator.key().startsWith("login") ) {
				QString passEntry = "password";
				if( entriesIterator.key().size() > 5 ) // eg: login-2, login-3, etc
					passEntry += entriesIterator.key().mid(5); // eg: password-2, password-3, etc

				temp = QtString2NSString( entriesIterator.key() );
				PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindRealmLogins() login key: %s", NS_ConvertUTF16toUTF8(temp).get() ) );

				temp = QtString2NSString( entriesIterator.value() );
				PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindRealmLogins() login: %s", NS_ConvertUTF16toUTF8(temp).get() ) );

				temp = QtString2NSString( passEntry );
				PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindRealmLogins() password key: %s", NS_ConvertUTF16toUTF8(temp).get() ) );

				if( entry.contains( passEntry ) ) {
					temp = QtString2NSString( entry.value( passEntry ) );
					PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindRealmLogins() password: %s", NS_ConvertUTF16toUTF8(temp).get() ) );

					nsCOMPtr<nsILoginInfo> loginInfo = do_CreateInstance(NS_LOGININFO_CONTRACTID);
					if (!loginInfo)
						return NS_ERROR_FAILURE;

					loginInfo->SetHostname( QtString2NSString( hostname ) );
					loginInfo->SetHttpRealm( QtString2NSString( httpRealm ) );
					loginInfo->SetUsername(QtString2NSString( entriesIterator.value() ) );
					loginInfo->SetPassword(QtString2NSString( entry.value( passEntry ) ) );

					NS_ADDREF(loginInfo);
					array[i] = loginInfo;

					i++;
				}
			}
		}
	}

	if( (int)*count != i ) {
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindRealmLogins() something went wrong while counting passswords" ) );
		return NS_ERROR_FAILURE;
	}

	*logins = array;
	return NS_OK;
}

NS_IMETHODIMP GetLoginInfoFromLoginMap( const QString & passwordName, QMap< QString, QString > & loginMap, nsCOMPtr<nsILoginInfo> & loginInfo ) {
	nsresult res;
	loginInfo = do_CreateInstance(NS_LOGININFO_CONTRACTID, &res);
	NS_ENSURE_SUCCESS(res, res);
	if (!loginInfo)
		return NS_ERROR_FAILURE;
	if( loginMap.contains( passwordName ) ) {
		loginInfo->SetPasswordField( QtString2NSString( passwordName ) );
		loginInfo->SetPassword( QtString2NSString( loginMap.value( passwordName ) ) );
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetLoginInfoFromMapEntry() password field %s", passwordName.toUtf8().data() ) );
		loginMap.remove( passwordName );
		QMap<QString, QString>::const_iterator username = loginMap.constBegin();
		loginInfo->SetUsernameField( QtString2NSString( username.key() ) );
		loginInfo->SetUsername( QtString2NSString( username.value() ) );
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetLoginInfoFromMapEntry() login field %s", username.key().toUtf8().data() ) );
	}
	NS_ADDREF(loginInfo);

	return NS_OK;
}

NS_IMETHODIMP FindFormLogins(PRUint32 *count, const QString &url, nsILoginInfo ***logins) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindFormLogins() Called") );

	nsresult res = selectFormDataFolder();
	NS_ENSURE_SUCCESS(res, res);

	QString key = url;

	int index = url.indexOf("//");
	index = url.indexOf("/", index + 2); //Find the end of the host name
	QString hostname = url.left( index );
	nsAutoString aHostname = QtString2NSString( hostname );
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindFormLogins() hostname: %s", hostname.toUtf8().data() ) );

	// Firefox does not end URLs with /, we should add it
	if( key.count( "/" ) < 3 )
		key += "/";

	key += "#*";

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindFormLogins() key: %s", key.toUtf8().data() ) );
	QMap< QString, QMap< QString, QString > > entryMap;

	if( wallet->readMapList( key, entryMap ) != 0 )
		return NS_ERROR_FAILURE;
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindFormLogins() Found %d Form Data logins", entryMap.count() ) );

	if( entryMap.count() > 1 ) {
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindFormLogins() there is more than one password in Form Data" ) );
		return NS_ERROR_FAILURE;
	}

	res = selectDefaultFolder();
	NS_ENSURE_SUCCESS(res, res);

	key += " *";

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindFormLogins() key: %s", key.toUtf8().data() ) );

	QMap< QString, QMap< QString, QString > > privateMap;
	if( wallet->readMapList( key, privateMap ) != 0 )
		return NS_ERROR_FAILURE;
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindFormLogins() Found %d private logins", privateMap.count() ) );

	entryMap.unite( privateMap );

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindFormLogins() Found %d logins", entryMap.count() ) );

	QStringList formNames;
	QStringList passwordNames;

	res = GetFormsAndPasswordsNames( formNames, passwordNames );
	NS_ENSURE_SUCCESS(res, res);

	nsILoginInfo **array = (nsILoginInfo**) nsMemory::Alloc( entryMap.count() * sizeof(nsILoginInfo*));
	NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);
	memset(array, 0, entryMap.count() * sizeof(nsILoginInfo*));

	int j = 0;
	for( int i = 0; i < formNames.size(); i++) {
		QMapIterator< QString, QMap< QString, QString > > iterator(entryMap);

		QString fullUrl = url + "#" + formNames[i];
		while (iterator.hasNext()) {
			iterator.next();
			QString thisKey = iterator.key().section( ' ', 0, 0);			
			if( thisKey == fullUrl ) {
				nsCOMPtr<nsILoginInfo> loginInfo;
				QMap< QString, QString > loginMap = iterator.value();
				res = GetLoginInfoFromLoginMap( passwordNames[i], loginMap, loginInfo );
				NS_ENSURE_SUCCESS(res, res);
				loginInfo->SetHostname( aHostname );
				loginInfo->SetFormSubmitURL( aHostname );
				PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindFormLogins() Adding info for %s", fullUrl.toUtf8().data() ) );

				array[j] = loginInfo;
				j++;
			}
		}
	}

	if( entryMap.count() != j ) {
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindFormLogins() %d %d", entryMap.count(), j ) );
		NS_ERROR("FindFormLogins found an inconsistency in password count");
		return NS_ERROR_FAILURE;
	}

	*count = j;
	*logins = array;
	return NS_OK;
}

NS_IMETHODIMP FindBulkLogins(PRUint32 *count,
		const nsAString & aHostname,
		const nsAString & aActionURL,
		const nsAString & aHttpRealm,
		nsILoginInfo ***logins) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindBulkLogins() Called") );
	*count = 0;

	nsresult res = selectDefaultFolder();
	NS_ENSURE_SUCCESS(res, res);

	QString key = generateBulkWalletSearchKey( aHostname, aActionURL, aHttpRealm, NS_ConvertUTF8toUTF16( "*" ) );
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindBulkLogins() key: %s", key.toUtf8().data() ) );

	QMap< QString, QMap< QString, QString > > entryMap;
	if( wallet->readMapList( key, entryMap ) != 0 )
		return NS_OK;
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindBulkLogins() Found %d maps", entryMap.count() ) );
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

NS_IMETHODIMP TryToEnableBulkLogin(const nsAString &, const nsAString &, const nsAString &);

NS_IMETHODIMP KDEWallet::FindLogins(PRUint32 *count,
                                       const nsAString & aHostname,
                                       const nsAString & aActionURL,
                                       const nsAString & aHttpRealm,
                                       nsILoginInfo ***logins) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindLogins() Called") );

	TryToEnableBulkLogin( aHostname, aActionURL, aHttpRealm );

	if( aActionURL.IsVoid() )
		return FindRealmLogins( count, aHostname, aHttpRealm, logins );

	if( aHttpRealm.IsVoid() ) {
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindLogins() hostname: %s", NS_ConvertUTF16toUTF8(aHostname).get() ) );
		QString url;
		nsresult res = GetURL( url );
		if( res != NS_OK ) // We cannot acces DOM, find bulk logins
			return FindBulkLogins( count, aHostname, aActionURL, aHttpRealm, logins );

		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindLogins() url: %s", url.toUtf8().data() ) );
		return FindFormLogins( count, url, logins );
	}

	NS_ERROR("FindLogins must set aActionURL or aHttpRealm");
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP FindAllFormLogins(PRUint32 *count, nsILoginInfo ***logins) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindAllFormLogins() Called") );

	nsresult res = selectFormDataFolder();
	NS_ENSURE_SUCCESS(res, res);

	QMap< QString, QMap< QString, QString > > entryMap;

	if( wallet->readMapList( "*", entryMap ) != 0 )
		return NS_ERROR_FAILURE;
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindAllFormLogins() Found %d Form Data logins", entryMap.count() ) );

	res = selectDefaultFolder();
	NS_ENSURE_SUCCESS(res, res);

	QMap< QString, QMap< QString, QString > > privateMap;
	if( wallet->readMapList( "* *", privateMap ) != 0 )
		return NS_ERROR_FAILURE;
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindAllFormLogins() Found %d private logins", privateMap.count() ) );

	entryMap.unite( privateMap );

	nsILoginInfo **array = (nsILoginInfo**) nsMemory::Alloc( entryMap.count() * sizeof(nsILoginInfo*));
	NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);
	memset(array, 0, entryMap.count() * sizeof(nsILoginInfo*));

	int j = 0;
	QMapIterator< QString, QMap< QString, QString > > iterator(entryMap);

	while (iterator.hasNext()) {
		iterator.next();
		nsCOMPtr<nsILoginInfo> loginInfo;
		loginInfo = do_CreateInstance(NS_LOGININFO_CONTRACTID, &res);
		NS_ENSURE_SUCCESS(res, res);
		if (!loginInfo)
			return NS_ERROR_FAILURE;

		QString url = iterator.key();
		int index = url.indexOf("//");
		index = url.indexOf("/", index + 2); //Find the end of the host name
		QString hostname = url.left( index );
		nsAutoString aHostname = QtString2NSString( hostname );
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::FindAllFormLogins() hostname: %s", hostname.toUtf8().data() ) );

		loginInfo->SetHostname( aHostname );
		loginInfo->SetFormSubmitURL( aHostname );
		QMap< QString, QString > loginMap = iterator.value();
		loginInfo->SetUsername( QtString2NSString( QStringList( loginMap.keys() ).join("|") ) );
		loginInfo->SetPassword( QtString2NSString( QStringList( loginMap.values() ).join("|") ) );

		NS_ADDREF(loginInfo);
		array[j] = loginInfo;
		j++;
	}

	if( entryMap.count() != j ) {
		NS_ERROR("FindAllFormLogins found an inconsistency in password count");
		return NS_ERROR_FAILURE;
	}

	*count = j;
	*logins = array;
	return NS_OK;
}

NS_IMETHODIMP CountRealmLogins(const nsAString & aHostname, 
                                        const nsAString & aHttpRealm,
                                        PRUint32 *_retval) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountRealmLogins() Called") );

	*_retval = 0;
	QString key;

	nsresult res = generateRealmWalletSearchKey( aHostname, aHttpRealm, key );
	NS_ENSURE_SUCCESS(res, res);
	
	nsAutoString temp = QtString2NSString( key );	
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountRealmLogins() search key: %s", NS_ConvertUTF16toUTF8(temp).get() ) );
	
	res = selectPasswordFolder();
	NS_ENSURE_SUCCESS(res, res);
		
	QMap< QString, QMap< QString, QString > > entryMap;
	if( wallet->readMapList( key, entryMap ) != 0 ) 
		return NS_OK;
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountRealmLogins() Found %d maps", entryMap.count() ) );

	if( entryMap.count() == 0 ) 
		return NS_OK;

	QMapIterator< QString, QMap< QString, QString > > iterator(entryMap);
	int i = 0;
	while (iterator.hasNext()) {
		iterator.next();
		
		temp = QtString2NSString( iterator.key() );
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountRealmLogins() key: %s", NS_ConvertUTF16toUTF8(temp).get() ) );

 		QMap< QString, QString > entry = iterator.value();			
		QMapIterator< QString, QString > entriesIterator(entry);
		while (entriesIterator.hasNext()) {
			entriesIterator.next();
			if( entriesIterator.key().startsWith("login") ) {
				QString passEntry = "password";
				if( entriesIterator.key().size() > 5 ) // eg: login-2, login-3, etc
					passEntry += entriesIterator.key().mid(5); // eg: password-2, password-3, etc
								
				temp = QtString2NSString( entriesIterator.key() );
				PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountRealmLogins() login key: %s", NS_ConvertUTF16toUTF8(temp).get() ) );

				temp = QtString2NSString( entriesIterator.value() );
				PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountRealmLogins() login: %s", NS_ConvertUTF16toUTF8(temp).get() ) );

				temp = QtString2NSString( passEntry );
				PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountRealmLogins() password key: %s", NS_ConvertUTF16toUTF8(temp).get() ) );			
				
				if( entry.contains( passEntry ) ) {
					temp = QtString2NSString( entry.value( passEntry ) );
					PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountRealmLogins() password: %s", NS_ConvertUTF16toUTF8(temp).get() ) );			
					
					i++;
				}
			}
		}
		
	}	

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountRealmLogins() Found %d logins", i ) );

	*_retval = i;
	return NS_OK;
}

NS_IMETHODIMP CountFormLogins( PRUint32 *_retval) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountFormLogins() Called") );

	QStringList formNames;
	QStringList passwordNames;
	nsresult res = GetFormsAndPasswordsNames( formNames, passwordNames );
	NS_ENSURE_SUCCESS(res, res);

	QString url;
	res = GetURL( url );
	NS_ENSURE_SUCCESS(res, res);
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountFormLogins() url: %s", url.toUtf8().data() ) );
	
	*_retval = 0;
	
	for( int i = 0; i < formNames.size(); i++) {
		/* First we find in folder: Form Data */
		res = selectFormDataFolder();
		NS_ENSURE_SUCCESS(res, res);
		
		QString key = url; 
		
		// Firefox does not end URLs with /, we should add it
		if( key.count( "/" ) < 3 )
			key += "/";
		
		key += "#" + formNames[i];

		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountFormLogins() key: %s", key.toUtf8().data() ) );
		QMap< QString, QMap< QString, QString > > entryMap;

		if( wallet->readMapList( key, entryMap ) != 0 ) 
			return NS_ERROR_FAILURE;
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountFormLogins() Found %d Form Data logins", entryMap.count() ) );

		if( entryMap.count() > 1 ) {
			PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountFormLogins() Form Data can not have more than one password" ) );
			return NS_ERROR_FAILURE;
		}
		*_retval += entryMap.count();
		
		
		res = selectDefaultFolder();
		NS_ENSURE_SUCCESS(res, res);
		
		key += " *";
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountFormLogins() key: %s", key.toUtf8().data() ) );

		QMap< QString, QMap< QString, QString > > privateMap;
		if( wallet->readMapList( key, privateMap ) != 0 ) 
			return NS_ERROR_FAILURE;
		
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountFormLogins() Found %d private logins", privateMap.count() ) );
		*_retval += privateMap.count();

		QMapIterator< QString, QMap< QString, QString > > iterator(privateMap);
		while (iterator.hasNext()) {
			iterator.next();
			PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountFormLogins() key %s", iterator.key().toUtf8().data() ) );
		}

	}
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountFormLogins() Found %d logins", *_retval ) );
	return NS_OK;
}

NS_IMETHODIMP AddFormLogin(nsILoginInfo *);
NS_IMETHODIMP AddRealmLogin(nsILoginInfo *);

NS_IMETHODIMP TryToEnableBulkLogin(const nsAString & aHostname,
				const nsAString & aActionURL,
				const nsAString & aHttpRealm) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::TryToEnableBulkLogin() Called") );

	PRUint32 count;
	nsILoginInfo **logins;
	nsresult res = FindBulkLogins( &count, aHostname, aActionURL, aHttpRealm, &logins );
	NS_ENSURE_SUCCESS(res, res);

	PRUint32 i;
	for( i = 0; i < count; i++ ) {
		nsAutoString aHttpRealm;
		logins[i]->GetHttpRealm(aHttpRealm);
		if( aHttpRealm.IsVoid() )
			res = AddFormLogin( logins[i] );
		else 
			res = AddRealmLogin( logins[i] );
		NS_ENSURE_SUCCESS(res, res);

		res = RemoveBulkLogin( logins[i] );
		NS_ENSURE_SUCCESS(res, res);
	}
	
	return NS_OK;
}

NS_IMETHODIMP KDEWallet::CountLogins(const nsAString & aHostname,
                                        const nsAString & aActionURL,
                                        const nsAString & aHttpRealm,
                                        PRUint32 *_retval) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::CountLogins() Called") );

	TryToEnableBulkLogin( aHostname, aActionURL, aHttpRealm );

	if( aActionURL.IsVoid() )
		return CountRealmLogins( aHostname, aHttpRealm, _retval );
	
	if( aHttpRealm.IsVoid() )
		return CountFormLogins( _retval );

	NS_ERROR("CountLogins must set aActionURL or aHttpRealm");
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP AddRealmLogin(nsILoginInfo *aLogin) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::AddRealmLogin() Called") );
	
	nsAutoString aHostname;
	aLogin->GetHostname(aHostname);
	nsAutoString aHttpRealm;
	aLogin->GetHttpRealm(aHttpRealm);
	QString key;

	nsresult res = generateRealmWalletKey( aHostname, aHttpRealm, key );
	NS_ENSURE_SUCCESS(res, res);
	
	nsAutoString temp = QtString2NSString( key );
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::AddRealmLogin() key: %s", NS_ConvertUTF16toUTF8(temp).get() ) );
	
	res = selectPasswordFolder();
	NS_ENSURE_SUCCESS(res, res);

	QMap< QString, QString > entry;
	wallet->readMap( key, entry );

	QString passwordField( "password" );
	QString s;

	/* Find first available entry of type password-num */
	int numEntry = 0;
	while( entry.contains( passwordField ) ) {
		numEntry++;
		s = QString( "-%1" ).arg( numEntry );
		passwordField = "password" + s;
	}

	/* Prepare the login field with the same entry */
	QString loginField( "login" );
	loginField += s;

	nsAutoString aUsername;
	aLogin->GetUsername(aUsername);
	entry[ loginField ] = NSString2QtString(aUsername);

	nsAutoString aPassword;
	aLogin->GetPassword(aPassword);
	entry[ passwordField ] = NSString2QtString(aPassword);
		
	if( wallet->writeMap( key, entry ) ) {
		NS_ERROR("Can not save map information");
		return NS_ERROR_FAILURE;
	}
	return NS_OK;
}

NS_IMETHODIMP AddBulkLogin(nsILoginInfo *aLogin) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::AddBulkLogin() Called") );
  
	nsAutoString s;
	QMap< QString, QString > entry;

	nsAutoString aUsername;
	aLogin->GetUsername(aUsername);
	entry[ kUsernameAttr ] = NSString2QtString(aUsername);
	
	aLogin->GetPassword(s);
	entry[ kPasswordAttr ] = NSString2QtString(s);
	
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

	nsresult res = selectDefaultFolder();
	NS_ENSURE_SUCCESS(res, res);

	QString key = generateBulkWalletKey( aHostname, aActionURL, aHttpRealm, aUsername );
	if( wallet->writeMap( key, entry ) ) {
		NS_ERROR("Can not save map information");
		return NS_ERROR_FAILURE;
	}
	return NS_OK;
}

NS_IMETHODIMP AddFormLogin(nsILoginInfo *aLogin) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::AddFormLogin() Called") );
		
	QStringList formNames;
	QStringList passwordNames;
	
	nsresult res = GetFormsAndPasswordsNames( formNames, passwordNames );
	
	if( res != NS_OK ) // We are doing a bulk add, we havn't got DOM access, we add data in a special way
		return AddBulkLogin( aLogin );

	nsAutoString aPasswordField;
	aLogin->GetPasswordField(aPasswordField);
	QString passwordField = NSString2QtString(aPasswordField);

	QString formName;
	// Select the form we are saving
	for( int i = 0; i < passwordNames.size(); i++) {
		if( passwordNames[i] == passwordField ) {
			formName = formNames[i];
			break;
		}
	}
	
	PRUint32 count;
	res = CountFormLogins( &count );
	NS_ENSURE_SUCCESS(res, res);

	nsAutoString s;
	QMap< QString, QString > entry;

	aLogin->GetUsernameField(s);
	nsAutoString aUsername;
	aLogin->GetUsername(aUsername);
	entry[ NSString2QtString(s) ] = NSString2QtString(aUsername);
	
	aLogin->GetPassword(s);
	entry[ passwordField ] = NSString2QtString(s);
	
	QString url;
	res = GetURL( url );
	NS_ENSURE_SUCCESS(res, res);

	QString key = url; 
	
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::AddFormLogin() url: %s", url.toUtf8().data() ) );

	// Firefox does not end URLs with /, we should add it
	if( key.count( "/" ) < 3 )
		key += "/";
	
	key += "#" + formName;

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::AddFormLogin() key: %s", key.toUtf8().data() ) );

	/* The first login goes to Form Data, others go to Firefox folder */
	if( count ) {
		// Add password to Firefox folder
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::AddFormLogin() Add password to Firefox folder") );

		res = selectDefaultFolder();
		NS_ENSURE_SUCCESS(res, res);

		key += " " + NSString2QtString(aUsername);
	}
	else {
		// Add password to Form Data folder
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::AddFormLogin() Add password to Form Data folder") );

		res = selectFormDataFolder();
		NS_ENSURE_SUCCESS(res, res);
	}
	
	if( wallet->writeMap( key, entry ) ) {
		NS_ERROR("Can not save map information");
		return NS_ERROR_FAILURE;
	}
	return NS_OK;
}

NS_IMETHODIMP KDEWallet::AddLogin(nsILoginInfo *aLogin) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::AddLogin() Called") );

	nsAutoString aActionURL;
	aLogin->GetFormSubmitURL(aActionURL);
	if( aActionURL.IsVoid() )
		return AddRealmLogin( aLogin );

	nsAutoString aHttpRealm;
	aLogin->GetHttpRealm(aHttpRealm);
	if( aHttpRealm.IsVoid() )
		return AddFormLogin( aLogin );

	NS_ERROR("AddLogin must set aActionURL or aHttpRealm");
	return NS_ERROR_FAILURE;
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

	if( !wallet ) {
		wallet =  KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous );
		if( !wallet )
			return NS_ERROR_FAILURE;
	}

	if( wallet->hasFolder( "Form Data" ) ) {
		if( !wallet->removeFolder( "Form Data" ) )
			return NS_ERROR_FAILURE;
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveAllLogins() removed folder Form Data") );
	}

	if( wallet->hasFolder( "Passwords" ) ) {
		if( !wallet->removeFolder( "Passwords" ) )
			return NS_ERROR_FAILURE;
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveAllLogins() removed folder Passwords") );
	}

	QString folderPref;
	nsresult res = GetFolderPreference( folderPref );
	NS_ENSURE_SUCCESS(res, res);
	if( wallet->hasFolder( folderPref ) ) {
		if( !wallet->removeFolder( folderPref ) )
			return NS_ERROR_FAILURE;
		PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::RemoveAllLogins() removed folder %s", folderPref.toUtf8().data() ) );
	}

	return NS_OK;
}

NS_IMETHODIMP KDEWallet::GetAllLogins(PRUint32 *aCount, nsILoginInfo ***aLogins) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetAllLogins() Called") );

	*aCount = 0;

	PRUint32 realmCount;
	nsILoginInfo **realmLogins;
	nsresult res = FindRealmLogins( &realmCount, NS_ConvertASCIItoUTF16("*"), NS_ConvertASCIItoUTF16("*"), &realmLogins  );
	NS_ENSURE_SUCCESS(res, res);
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetAllLogins() Found %d Realm Logins", realmCount ) );
	if( realmCount > 0 ) {
		*aCount = realmCount;
		*aLogins = realmLogins;
	}
	
	PRUint32 formCount;
	nsILoginInfo **formLogins;
	res = FindAllFormLogins( &formCount, &formLogins  );
	NS_ENSURE_SUCCESS(res, res);
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetAllLogins() Found %d Form Logins", formCount ) );
	if( formCount > 0 ) {
		if( *aCount == 0 ) {
			*aCount = formCount;
			*aLogins = formLogins;
		} else {
			PRUint32 count = *aCount + formCount;
			nsILoginInfo **array = (nsILoginInfo**) nsMemory::Alloc( count * sizeof(nsILoginInfo*));
			NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);
			memcpy( array, *aLogins, *aCount * sizeof(nsILoginInfo*));
			nsMemory::Free( *aLogins );
			memcpy(array + *aCount, formLogins, formCount * sizeof(nsILoginInfo*));
			nsMemory::Free( formLogins );
			*aCount = count;
			*aLogins = array;
		}
	}
		
	PRUint32 bulkCount;
	nsILoginInfo **bulkLogins;
	res = FindBulkLogins( &bulkCount, NS_ConvertASCIItoUTF16("*"), NS_ConvertASCIItoUTF16("*"), NS_ConvertASCIItoUTF16("*"), &bulkLogins  );
	NS_ENSURE_SUCCESS(res, res);
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetAllLogins() Found %d Bulk Logins", bulkCount ) );
	if( bulkCount > 0 ) {
		if( *aCount == 0 ) {
			*aCount = bulkCount;
			*aLogins = bulkLogins;
		} else {
			PRUint32 count = *aCount + bulkCount;
			nsILoginInfo **array = (nsILoginInfo**) nsMemory::Alloc( count * sizeof(nsILoginInfo*));
			NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);
			memcpy( array, *aLogins, *aCount * sizeof(nsILoginInfo*));
			nsMemory::Free( *aLogins );
			memcpy(array + *aCount, bulkLogins, bulkCount * sizeof(nsILoginInfo*));
			nsMemory::Free( bulkLogins );
			*aCount = count;
			*aLogins = array;
		}
	}

	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetAllLogins() Found %d Logins", *aCount ) );
	return NS_OK;
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
  PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetAllEncryptedLogins() unimplemented") );
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP KDEWallet::GetAllDisabledHosts(PRUint32 *aCount,
                                                PRUnichar ***aHostnames) {
	PR_LOG( gKDEWalletLog, PR_LOG_DEBUG, ( "KDEWallet::GetAllDisabledHosts() Called") );
	*aCount = 0;

	nsresult res = selectDefaultFolder();
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
  
	nsresult res = selectDefaultFolder();
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
  
	nsresult res = selectDefaultFolder();
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


/* End of implementation class template. */

static NS_METHOD
KDEWalletRegisterSelf(nsIComponentManager *compMgr, nsIFile *path,
                         const char *loaderStr, const char *type,
                         const nsModuleComponentInfo *info) {
  nsCOMPtr<nsICategoryManager> cat = do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
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
 
