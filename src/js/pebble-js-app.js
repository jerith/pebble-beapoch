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
    var offset_number = (offset_hours * 100) + offset_minutes;

    // Make a string with zero padding.
    var offset_str = offset_number.toString();
    if (offset_str.length == 3) {
        offset_str = "0" + offset_str;
    }

    if (is_negative) {
        offset_str = "-" + offset_str;
    } else {
        offset_str = "+" + offset_str;
    }

    console.log(logstr + " offset_str='" + offset_str + "'");
    return offset_str;
}

Pebble.addEventListener(
    "ready", function(e) {
        console.log("*** ready");
        // Always send the UTC offset on startup.
        // The watch may have cached it, but we could be in a different timezone.
        Pebble.sendAppMessage({"utc_offset": getUTCOffset()});
    }
);

Pebble.addEventListener(
    "appmessage", function(e) {
        var logstr = "*** appmessage";
        if (e.payload.utc_offset) {
            console.log(logstr + " (utc_offset request)");
            Pebble.sendAppMessage({"utc_offset": getUTCOffset()});
        }
    }
);
