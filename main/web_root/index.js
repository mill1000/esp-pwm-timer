// Workaround issue #2858 https://github.com/olifolkerd/tabulator/issues/2858
Tabulator.prototype.moduleBindings.edit.prototype.clearEdited = function (cell) {
  if (cell.modules.edit && cell.modules.edit.edited) {
    if (cell.modules.validate && cell.modules.validate.invalid) {
      cell.modules.validate.invalid = false;
    }

    let editIndex = this.editedCells.indexOf(cell);

    if (editIndex > -1) {
      this.editedCells.splice(editIndex, 1);
    }
  }
};

function validId(id) {
  // We want 0 as a valid ID but want to exlude null, undef and empty
  return id != undefined && id != null && id !== "";
}

// Class to represent a timer configuration
class Timer {
  constructor(opts = {}) {
    this.id = opts.id
    this.freq = opts.freq || 500;
  }

  get valid() { return this.freq ? true : false; }

  get name() { 
    return "Timer " + this.id;
  }

  set name(v) {
    null;
  }
}

// "Namespace" to handle timer array
var Timers = {
  _timers: Array.from(Array(4).keys()).map(x => new Timer({id: x})),
  
  get all() {
    return this._timers;
  },

  get select_list() {
    return Timers.all.map(t => t.id);
  },

  get_name: function(id) {
    return validId(id) ? this._timers[id].name : "";
  },

  get dictionary() {
    // Fetch the timer table as a dictionary
    let obj = {};
    this.all.forEach(t => obj[t.id] = t);
    return obj;
  },

  from_dictionary: function(dict) {
    let arr = [];
    Object.values(dict).forEach(v => arr.push(new Timer(v)));
    return this._timers = arr;
  },
}

// Class to represent a channel configuration
class Channel {
  constructor(opts = {}) {
    this.id = opts.id;
    this.name = opts.name || "Channel " + opts.id;
    this.timer = opts.timer || 0;
    this.gpio = opts.gpio || null;
    this.enabled = opts.enabled || false;
  }

  get columnDefinition() {
    // Generate a Tabulator column def
    return {
      title: "Channel " + this.id + "<br/>" + this.name,
      field: "" + this.id, // int -> string
      editor: "number", editorParams: { min: 0, max: 100, step: 10, mask: "999" },
      validator: "max:100",
      hozAlign: "center",
      minWidth: "100em",
      headerContextMenu:
        [
          { label: "Add Sweep", action: generateSweep, },
        ],
    };
  }

  get valid() { return validId(this.gpio) && validId(this.timer); }
}

// "Namespace" to handle channel array
var Channels = {
  _channels: Array.from(Array(8).keys()).map(x => new Channel({id: x})),
  
  get all() {
    return this._channels;
  },

  get enabled() {
    return this._channels.filter(c => c.enabled);
  },

  get columns() {
    return this.enabled.map(c => c.columnDefinition);
  },

  get dictionary() {
    let obj = {};
    this.all.forEach(c => obj[c.id] = c);
    return obj;
  },

  from_dictionary: function(dict) {
    let arr = [];
    Object.values(dict).forEach(v => arr.push(new Channel(v)));
    return this._channels = arr;
  },
}

// "Namespace" to handle channel array
var Schedule = {
   _data: [],
  
  get data() {
    return this._data;
  },

  set data(val) {
    this._data = val;
  },

  get dictionary() {
    let obj = {};
    this.data.forEach(c => obj[c.tod] = c);
    return obj;
  },

  from_dictionary: function(dict) {
    let arr = [];
    Object.values(dict).forEach(v => arr.push(v));
    return this.data = arr;
  },
}

function timeEditor(cell, onRendered, success, cancel, editorParams) {
  let editor = document.createElement("input");

  editor.setAttribute("id", "timepicker");
  editor.setAttribute("type", "time");

  onRendered(function () {
    // Attach a time picker tothe editor
    let picker = flatpickr("#" + editor.id, {
      enableTime: true,
      noCalendar: true,
      dateFormat: "H:i",
      defaultDate: cell.getValue(),
      onClose: () => success(editor.value),
    });

    // Open the editor
    picker.open();
  });

  return editor;
}

var Status = {
  _element: document.getElementById("status"),

  set: function (message) {
    this._element.innerHTML = message;
  },

  set_success: function (message) {
    this.set(message);
  },

  set_error: function (message) {
    this.set("Save failed: " + message);
  },
}

function sendJsonXhrRequest(json, callback) {
  if (typeof josn != "string")
    json = JSON.stringify(json);

  // Hack to keep empty strings from tabulator from crash json parser
  json = json.replace(/\:\"\"/gi, ":null");

  let xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function () {
    if (xhr.readyState == XMLHttpRequest.DONE) {
      callback(xhr.status)
    }
  };

  xhr.open("POST", window.location.href + "/?action=set");
  xhr.timeout = 5000;
  xhr.setRequestHeader("Content-Type", "application/json");
  xhr.send(json);
}

function getJsonXhrRequest() {
  let promise = new Promise((resolve, reject) => {
    let xhr = new XMLHttpRequest();
    xhr.onreadystatechange = () => {
      if (xhr.readyState == XMLHttpRequest.DONE) {
        if (xhr.status == 200) {
          resolve(JSON.parse(xhr.responseText));
        }
        else {
          message = "Failed to load settings. Error: "
          message += (xhr.status != 0) ? xhr.responseText : "Timeout";
          reject(message);
        }
      }
    };

    xhr.open("GET", window.location.href + "/?action=get");
    xhr.timeout = 5000;
    xhr.send();
  });

  return promise;
}

function save() {
  // Force validation of TOD fields
  scheduleTable.validate("tod");

  if (timerTable.getInvalidCells().length)
  {
    Status.set_error("Invalid timer setup. Please fix errors.");
    return;
  }

  if (channelTable.getInvalidCells().length)
  {
    Status.set_error("Invalid channel setup. Please fix errors.");
    return;
  }

  if (scheduleTable.getInvalidCells().length)
  {
    Status.set_error("Invalid schedule. Please fix errors.");
    return;
  }

  // Copy table data into our schedule
  Schedule.data = scheduleTable.getData();

  let settings = {};
  settings.timers = Timers.dictionary;
  settings.channels = Channels.dictionary;
  settings.schedule = Schedule.dictionary;

  console.log("Timers:" + JSON.stringify(settings.timers));
  console.log("Channels:" + JSON.stringify(settings.channels));
  console.log("Schedule:" + JSON.stringify(settings.schedule));

  function setCallback(status) {
    if (status == 200)
      Status.set_success("Complete.");
    else {
      message = "Failed. Error: ";
      message += (status != 0) ? xhr.responseText : "Timeout";
      Status.set_error(message);
    }
  }

  Status.set("Sending settings...");
  sendJsonXhrRequest(settings, setCallback);
}

function load() {
  Status.set("Loading settings...");
  getJsonXhrRequest().catch((message) => {
    Status.set_error(message);
  }).then((settings) => {
    timerTable.setData(Timers.from_dictionary(settings.timers));
    channelTable.setData(Channels.from_dictionary(settings.channels));

    // Manually trigger column update dumb!
    scheduleTable.setColumns(columnTemplate.concat(Channels.columns));

    scheduleTable.replaceData(Schedule.from_dictionary(settings.schedule));

    Status.set_success("Settings loaded.");
  });
}

function* range(start, end, step) {
  if (step == 0 || start == end)
    return;
    
  for (let i = start; i <= end; i+=step) {
      yield i;
  }
}

function cubic_interpolate(deltaY, x) {
  return (3 * deltaY * (x ** 2)) - (2 * deltaY * (x ** 3))
}

function linear_interpolate(deltaY, x) {
  return deltaY * x;
}