var hash_stations = []

function getElementsByClassName(className)
{
    var tags, e, acc;

    tags = document.getElementsByTagName("*");
    acc = [];
    for (var i in tags) {
        e = tags[i];
        if (e.className == undefined)
            continue;
        if(e.className.indexOf(className) != -1)
            acc.push(e);
    }

    return acc;
}

function request(cmd, arg, cb)
{
    var req;
    if (arg)
        req = ["command.cgi?command=", cmd, "&arg=", arg].join("");
    else
        req = ["command.cgi?command=", cmd].join("");
    var xmlhttp = new XMLHttpRequest();
    xmlhttp.open("GET", req);
    if (cb)
        xmlhttp.onreadystatechange = function() { if (xmlhttp.readyState == 4) cb(xmlhttp); };
    xmlhttp.send(null);

    return xmlhttp;
}

function show_playing(show)
{
    var playings, display;

    if (show)
        display = "";
    else
        display = "none";

    playings = getElementsByClassName("playing");
    for (i in playings)
        playings[i].style.display = display;
}

function set_tuned_cb(xmlhttp)
{
    var tuned, tuning, obj, station;
    var title, display;
    var nowplaying;
    var i;

    if ((tuning = document.getElementById("tuning")) != null)
        tuning.removeAttribute("id");
    if ((tuned = document.getElementById("tuned")) != null)
        tuned.removeAttribute("id");

    obj = eval(["(", xmlhttp.responseText, ")"].join(""));
    if ((station = hash_stations[obj["icy-name"]]) != undefined)
        station.elm.setAttribute("id", "tuned");

    if ((title = obj["icy-meta"]) == undefined)
        show_playing(false);
    else
        show_playing(true);

    if (title == undefined)
        return;

    if (title.indexOf("StreamTitle='") == 0)
        title = title.substring(13);
    if ((i = title.indexOf("';")) != -1)
        title = title.substring(0, i);
    if (title == "")
        show_playing(false);

    nowplaying = document.getElementById("nowplaying");
    nowplaying.innerHTML = title;       // XSS?
    nowplaying.setAttribute("href", "http://www.google.com/search?ie=utf-8&oe=utf-8&q=" + escape(title));

    setTimeout(function() { window.scrollTo(0, 1); }, 100);
}

function set_tuned()
{
    request("status", null, set_tuned_cb);
}

function tune(obj)
{
    var name, e;

    if ((e = document.getElementById("tuned")) != null)
        e.removeAttribute("id");
    obj.setAttribute("id", "tuning");
    name = obj["icy-name"];
    request("tune", hash_stations[name].URL, set_tuned_cb);
}

function stop()
{
    var e;

    if ((e = document.getElementById("tuned")) != null)
        e.removeAttribute("id");
    request("stop", null, null);
}

function get_genre(obj)
{
    var g, e, li, a, h;

    g = "genre-" + obj.genre;
    if ((e = document.getElementById(g)) != null)
        return e;

    e = document.createElement("ul");
    e.setAttribute("id", g);
    e.setAttribute("title", obj.genre);

    li = document.createElement("li");
    a = document.createElement("a");
    a.setAttribute("href", "#" + g);
    a.innerHTML = obj.genre;
    li.appendChild(a);
    h = document.getElementById("home");
    h.appendChild(li);
    document.body.appendChild(e);

    return e;
}

function append(s)
{
    var g = get_genre(s);
    var e = document.createElement("li");
    e = document.createElement("li");
    e["icy-name"] = s["icy-name"];
    e.innerHTML = s["display-name"] || s["icy-name"];
    e.setAttribute("onclick", "javascript:tune(this);");
    s.elm = e;
    g.appendChild(e);
}

function init()
{
    var ul, e, s;

    ul = document.getElementById("home");
    request("list", null,
        function (xmlhttp) {
            var stations = eval(["(", xmlhttp.responseText, ")"].join(""));
            hash_stations = [];
            for (var i in stations) {
                s = stations[i];
                hash_stations[s["icy-name"]] = s;
                if (s["display-name"])
                    hash_stations[s["display-name"]] = s;
                append(s);
            }
            set_tuned();
        });
}
