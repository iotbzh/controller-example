--[[
    Copyright (C) 2018 "IoT.bzh"
    Author Romain Forlot <romain.forlot@iot.bzh>

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.


    NOTE: strict mode: every global variables should be prefixed by '_'
--]]



function _launch_init() --Function ran by onload on startup
	print("~~~~~~~~~~ Onload Initialisation ~~~~~~~~~~ \n.\n.\n.\n.\n.\n.\n~~~~~~~~~~ End Initialisation ~~~~~~~~~~")
end

function _launch_example(source,args) -- Function ran by the ctrl_eg verb
  print("~~~~~~~~~~ Controller example launched ~~~~~~~~~~\n.\n.\n.\n.\n.\n.\n ~~~~~~~~~~ End Example ~~~~~~~~~~")
  AFB:servsync(source,"hello","eventadd",{tag = 'event',name = "LaunchExampleEvent"})
  AFB:servsync(source,"hello","eventsub",{tag = 'event'})
  AFB:servsync(source,"hello","eventpush",{tag = 'event'})
  AFB:success(source)
end

function _event(source,args,response) -- Function ran when event received
  print("\n~~~~~ Launch Example Event has been received ~~~~~ \n.\n.\n.\n.\n.\n.\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n")
end