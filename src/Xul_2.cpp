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

#include "mozilla/ModuleUtils.h"
#include "KDEWallet.h"


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

NS_IMPL_MOZILLA192_NSGETMODULE(&kKDEWalletModule)