// ==UserScript==
// @include http://dir.xiph.org/*
// ==/UserScript==

(function () {
    var baseurl = "http://www.home.magical-technology.com/hiroya/netrad/tune.cgi?url=http://dir.xiph.org";
    var getElementsByFormat = function(format) {
        var path = ["//td[@class='tune-in']/p/a[text()='", format, "']"].join("");
        return document.evaluate(path, document, null, 6, null);
    };
    var replace = function (elms) {
	var a;
        for (var i = 0; i < elms.snapshotLength; i++) {
            a = elms.snapshotItem(i);
            a.removeAttribute("onclick");
            href = a.getAttribute("href");
            a.setAttribute("href", baseurl + href);
        }
    };

    var xspf = getElementsByFormat("XSPF");
    replace(xspf);

    var m3u = getElementsByFormat("M3U");
    replace(m3u);
})();
