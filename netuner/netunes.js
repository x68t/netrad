function load()
{
    setNowplaying();
}

function stop()
{
    var req = new XMLHttpRequest();
    req.open('GET', 'command.cgi?command=stop', true);
    req.send(null);
}

function tune(li, URL)
{
    var req = new XMLHttpRequest();
    req.open('GET', 'tune.cgi?url=' + URL, true);
    req.send(null);
    li.setAttribute('selected', 'progress');
    setTimeout(setNowplaying, 5000, li);
}

function extractTitle(title)
{
    if (!title)
	return '';

    if (title.indexOf("StreamTitle='") == 0)
        title = title.substring(13);
    if ((i = title.indexOf("';")) != -1)
        title = title.substring(0, i);

    return title;
}

function setNowplaying(li)
{
    var req = new XMLHttpRequest();
    req.onreadystatechange = function() {
	if (req.readyState != 4)
	    return;
	var obj = eval(['(', req.responseText, ')'].join(''));
	var name = document.getElementById('icy-name');
	var meta = document.getElementById('icy-meta');
	name.innerHTML = obj['icy-name'] || 'no title';
	meta.innerHTML = extractTitle(obj['icy-meta']);
    };
    req.open('GET', 'command.cgi?command=status', true);
    req.send(null);
    if (li)
	li.removeAttribute('selected');
}
