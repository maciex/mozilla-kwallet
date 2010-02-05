firefox-kde-wallet.onFirefoxLoad = function(event) {
  document.getElementById("contentAreaContextMenu")
          .addEventListener("popupshowing", function (e){ firefox-kde-wallet.showFirefoxContextMenu(e); }, false);
};

firefox-kde-wallet.showFirefoxContextMenu = function(event) {
  // show or hide the menuitem based on what the context menu is on
  document.getElementById("context-firefox-kde-wallet").hidden = gContextMenu.onImage;
};

window.addEventListener("load", firefox-kde-wallet.onFirefoxLoad, false);
