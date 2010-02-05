var firefox-kde-wallet = {
  onLoad: function() {
    // initialization code
    this.initialized = true;
    this.strings = document.getElementById("firefox-kde-wallet-strings");
  },

  onMenuItemCommand: function(e) {
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                  .getService(Components.interfaces.nsIPromptService);
    promptService.alert(window, this.strings.getString("helloMessageTitle"),
                                this.strings.getString("helloMessage"));
  },

  onToolbarButtonCommand: function(e) {
    // just reuse the function above.  you can change this, obviously!
    firefox-kde-wallet.onMenuItemCommand(e);
  }
};

window.addEventListener("load", firefox-kde-wallet.onLoad, false);
