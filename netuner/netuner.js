function load()
{
    var req = new XMLHttpRequest();
    req.onreadystatechange = function() {
	var ul;
	if (req.readyState != 4)
	    return;
	ul = document.getElementById('netuner');
	ul.innerHTML = req.responseText;
	document.body.appendChild(ul);
    };
    req.open('GET', 'netuner.cgi?-12', true);
    req.send(null);
}

function tune(URL)
{
    var req = new XMLHttpRequest();
    req.open('GET', 'tune.cgi?url=' + URL, true);
    req.send(null);
}
