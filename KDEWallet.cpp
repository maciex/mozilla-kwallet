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
 * The Original Code is Gnome Keyring password manager storage.
 *
 * The Initial Developer of the Original Code is
 * Sylvain Pasche <sylvain.pasche@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Matt Lavin <matt.lavin@gmail.com>
 * Luca Niccoli <lultimouomo@gmail.com>
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

#pragma GCC visibility push(default)
extern "C" {
//#include "kde-wallet.h"
}
#pragma GCC visibility pop

#ifdef PR_LOGGING
PRLogModuleInfo *gKDEWalletLog;
#endif
/* create the preference item extensions.kde-wallet.keyringName
 * to set wich keyring save the password to. The default is mozilla.
 * Note that password will be retrieved from every unlocked keyring,
 * because the kde-wallet API doens't provide a way to search in
 * just one keyring.
 */
nsCString keyringName;

// XXX should use profile identifier instead of a constant
#define UNIQUE_PROFILE_ID "v1"

const char *kLoginInfoMagicAttrName = "mozLoginInfoMagic";
const char *kLoginInfoMagicAttrValue = "loginInfoMagic" UNIQUE_PROFILE_ID;

// For hostnames:
const char *kDisabledHostMagicAttrName = "mozDisabledHostMagic";
const char *kDisabledHostMagicAttrValue = "disabledHostMagic" UNIQUE_PROFILE_ID;

const char *kDisabledHostAttrName = "disabledHost";

const char *kHostnameAttr = "hostname";
const char *kFormSubmitURLAttr = "formSubmitURL";
const char *kHttpRealmAttr = "httpRealm";
const char *kUsernameFieldAttr = "usernameField";
const char *kPasswordFieldAttr = "passwordField";
const char *kUsernameAttr = "username";
const char *kPasswordAttr = "password";

/* Implementation file */
NS_IMPL_ISUPPORTS1(KDEWallet, nsILoginManagerStorage)

NS_IMETHODIMP KDEWallet::Init() {
  return NS_OK;
}

NS_IMETHODIMP KDEWallet::InitWithFile(nsIFile *aInputFile,
                                         nsIFile *aOutputFile) {
    return Init();
}

NS_IMETHODIMP KDEWallet::AddLogin(nsILoginInfo *aLogin) {
  return NS_OK;
}

NS_IMETHODIMP KDEWallet::RemoveLogin(nsILoginInfo *aLogin) {
  return NS_OK;
}

NS_IMETHODIMP KDEWallet::ModifyLogin(nsILoginInfo *oldLogin,
                                        nsISupports *modLogin) {
  /* If the second argument is an nsILoginInfo, 
   * just remove the old login and add the new one */

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
  return NS_OK;
}

NS_IMETHODIMP KDEWallet::GetAllLogins(PRUint32 *aCount,
                                         nsILoginInfo ***aLogins) {
  *aCount = 0;
  return NS_OK;
}

NS_IMETHODIMP KDEWallet::FindLogins(PRUint32 *count,
                                       const nsAString & aHostname,
                                       const nsAString & aActionURL,
                                       const nsAString & aHttpRealm,
                                       nsILoginInfo ***logins) {
  *count = 0;
  return NS_OK;
}

NS_IMETHODIMP KDEWallet::SearchLogins(PRUint32 *count,
                                         nsIPropertyBag *matchData,
                                         nsILoginInfo ***logins) {
  *count = 0;
  return NS_OK;
}

NS_IMETHODIMP KDEWallet::GetAllEncryptedLogins(unsigned int*,
                                                  nsILoginInfo***) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP KDEWallet::GetAllDisabledHosts(PRUint32 *aCount,
                                                PRUnichar ***aHostnames) {
  *aCount = 0;
  return NS_OK;
}

NS_IMETHODIMP KDEWallet::GetLoginSavingEnabled(const nsAString & aHost,
                                                  PRBool *_retval) {
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP KDEWallet::SetLoginSavingEnabled(const nsAString & aHost,
                                                  PRBool isEnabled) {
  return NS_OK;
}

NS_IMETHODIMP KDEWallet::CountLogins(const nsAString & aHostname, 
                                        const nsAString & aActionURL,
                                        const nsAString & aHttpRealm,
                                        PRUint32 *_retval) {
//  *_retval = count;
  *_retval = 0;
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
  nsCOMPtr<nsICategoryManager> cat =
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  NS_ENSURE_STATE(cat);

  cat->DeleteCategoryEntry("login-manager-storage", "nsILoginManagerStorage",
                           PR_TRUE);
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
 
