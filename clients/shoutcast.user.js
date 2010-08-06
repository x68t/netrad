// ==UserScript==
// @include http://www.shoutcast.com/*
// ==/UserScript==

(function() {
    var baseurl = "http://www.home.magical-technology.com/hiroya/netrad/tune.cgi?url=";
    var a, e;

    a = document.links;
    for (var i = 0; i < a.length; i++) {
        e = a[i];
        if (e.href.indexOf("http://yp.shoutcast.com/sbin/tunein-station.pls?") != 0)
	    continue;
	e.setAttribute("href", baseurl + e.href);
    }
})();
