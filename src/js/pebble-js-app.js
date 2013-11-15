Pebble.addEventListener(
    "ready", function(e) {
        console.log("*** ready");
    }
);

function getUTCOffset() {
    // This gives us our UTC offset in minutes, but negative.
    var tz_offset = (new Date()).getTimezoneOffset();
    var logstr = " ** getUTCOffset(): minutes=" + tz_offset;

    var is_negative = true;  // Reversed, because we have an offset in the wrong direction.
    if (tz_offset < 0) {
        is_negative = false;
        tz_offset *= -1;
    }

    // Now we have a positive offset, which is much nicer to work with.
    var offset_minutes = tz_offset % 60;
    var offset_hours = (tz_offset - offset_minutes) / 60;

    // Strings, with zero padding.
    var offset_minutes_str = offset_minutes.toString();
    if (offset_minutes_str.length == 1) {
        offset_minutes_str = "0" + offset_minutes_str;
    }
    var offset_hours_str = offset_hours.toString();
    if (offset_hours_str.length == 1) {
        offset_hours_str = "0" + offset_hours_str;
    }

    var offset_str = offset_hours_str + offset_minutes_str;
    if (is_negative) {
        offset_str = "-" + offset_str;
    } else {
        offset_str = "+" + offset_str;
    }

    console.log(logstr + " offset_str='" + offset_str + "'");
    return offset_str;
}

Pebble.addEventListener(
    "appmessage", function(e) {
        var logstr = "*** appmessage";
        if (e.payload.utc_offset) {
            console.log(logstr + " (utc_offset request)");
            Pebble.sendAppMessage({"utc_offset": getUTCOffset()});
        }
    }
);