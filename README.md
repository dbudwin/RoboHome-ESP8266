# RoboHome-ESP8266

## What Is RoboHome-ESP8266?

RoboHome-ESP8266 is the code that runs on an ESP8266 based microcontroller like the NodeMCU that will physically control semi-connected devices added using the [RoboHome-Web project](https://github.com/dbudwin/RoboHome-Web).

## What Is the RoboHome Project?

RoboHome is a SaaS tool that also integrates with Amazon's Echo to enable control of semi-connected devices (think IR, and RF) in your house over wifi! This is done using an MQTT pub-sub network to send messages between the website or Echo to a microcontroller like the ESP8266 which has various transmitters hooked up to it (like IR and RF transmitters) to send signals to these devices. This can be used to control RF outlets with lights plugged into them, or to turn on your TV and change channels for instance.

### Install Arduino Libraries :books:

Most of these can be downloaded from the Library Manager in the Ardunio IDE.  If it's not listed, follow the directions in the links below.

- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) for parsing JSON response from RoboHome-Web API
- [PubSub Client](https://github.com/knolleary/pubsubclient) for reading from the MQTT stream
- [RC-Switch](https://github.com/sui77/rc-switch) for sending signals to control RF devices
- [RestClient](https://github.com/DaKaZ/esp8266-restclient) for making API requests to the RoboHome-Web API 

### Configuring :wrench:

- Set values for all the variables at the top of the `RoboHome.ino` file to connect to your:
    - WiFi SSID and password
    - MQTT account (also in your RoboHome-Web `.env` file)
        - The topic should fit the pattern `RoboHome/<user_id>/+` where:
            - `RoboHome` is the top level topic to help distingish RoboHome traffic from other traffic on the MQTT account
            - `user_id` is the value for the [`user_id` field in the `users` table in the RoboHome-Web project](https://github.com/dbudwin/RoboHome-Web/blob/master/app/User.php) you want to control
            - `+` is a wildcard to match a single level
    - RoboHome-Web application URL and SSL/TLS SHA1 fingerprint
        - This can be found using Chrome's Developer Tools.  Click "Security," "View certificate," expand "Details," scroll to the "Fingerprints" section.  Copy the "SHA-1" value including spaces.
        - The value for `robohomeWebSha1Fingerprint` is likely to change whenever your SSL/TLS certificate gets renewed.  For [Let's Encrypt](https://www.letsencrypt.org/) users, the default is every 3 months.

### How To Contribute :gift:

Contributions are always welcome!  Please fork this repo and open a PR with your code or feel free to make an issue.  All PRs will need to be reviewed and pass automated checks.  If feedback is given on a PR, please submit updates to the original PR in the form of [fixup! commits](https://robots.thoughtbot.com/autosquashing-git-commits) which will later be squashed before the PR is merged.

This repo supports the principles of [Bob Martin's Clean Code](http://www.goodreads.com/book/show/3735293-clean-code).

### Notes :notebook:

The ESP8266 is a fairly weak computer which is why it is necessary to include the SSL/TLS certificate, otherwise it would be unable to connect to an HTTPS website which is required for the RoboHome-Web project.  Make sure the RoboHome-Web website the ESP8266 is connecting too uses the correct ciphers.  See the "Requirements" section of the RoboHome-Web README for up-to-date information for configuring the SSL/TLS certificate.
