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

String.prototype.format = function () {
	a = this;
	for (k in arguments) {
		a = a.replace("{" + k + "}", arguments[k])
	}
	return a
}

function nullOrEmpty(id) {
  // We want 0 as a valid ID but want to exlude null, undef and empty
  return id == undefined || id == null || id === "";
}

// Class to represent a timer configuration
class Timer {
  constructor(opts = {}) {
    this.id = opts.id
    this.freq = opts.freq || 500;
  }

  get valid() { return this.freq ? true : false; }

  get name() { 
    return "Timer {0}".format(this.id);
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
    return !nullOrEmpty(id) ? this._timers[id].name : "";
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
    this.name = opts.name || "Channel {0}".format(opts.id);
    this.timer = opts.timer || 0;
    this.gpio = opts.gpio || null;
    this.enabled = opts.enabled || false;
  }

  get columnDefinition() {
    // Generate a Tabulator column def
    return {
      title: "Channel {0}<br/>{1}".format(this.id, this.name),
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

  get valid() { return !nullOrEmpty(this.gpio) && !nullOrEmpty(this.timer); }
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

  get datasets() {
    // Construct a dataset for each enabled channel
    let datasets = Channels.enabled.map(c => ({ label: c.name, data: [] }));

    // Object to store the last set values for each ID
    let lastValues = {};

    this.data.forEach(row => {
      // Ignore row if no TOD assigned
      if (nullOrEmpty(row.tod))
        return;

      let ids = Object.keys(row).filter(x => x !== "tod");
      ids.forEach(id => {
        if (!nullOrEmpty(row[id]) && datasets[id]) {
          datasets[id].data.push({ x: row.tod, y: row[id] });
          lastValues[id] = row[id];
        }
      })
    });

    // Add points representing the wrap around behaviour
    for (const [id, value] of Object.entries(lastValues)) {
      datasets[id].data.push({ x: "24:00", y: value });
      datasets[id].data.unshift({ x: "00:00", y: value });
    }

    return datasets;
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
    // Attach a time picker to the editor
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

  set: function (message, detail = null) {
    let inner = message;
    if (detail)
      inner += "<br/><span class=\"subtext\">{0}</span>".format(detail)
    this._element.innerHTML = inner;
  },

  set_success: function (message, detail = null) {
    message = "&check; {0}".format(message)
    this.set(message, detail);
  },

  set_error: function (message, detail = null) {
    message = "&#x26A0; {0}".format(message)
    this.set(message, detail);
  },
}

function sendJsonXhrRequest(json, callback) {
  if (typeof josn != "string")
    json = JSON.stringify(json);

  // Hack to keep empty strings from tabulator crashing the json parser
  // when it expects a number
  json = json.replace(/\:\"\"/gi, ":null");

  let promise = new Promise((resolve, reject) => {
    let xhr = new XMLHttpRequest();
    xhr.onreadystatechange = () => {
      if (xhr.readyState == XMLHttpRequest.DONE) {
        if (xhr.status == 200) {
          resolve();
        }
        else {
          let message = "Error: {0}".format((xhr.status != 0) ? xhr.responseText : "Timeout");
          reject(message);
        }
      }
    };

    xhr.open("POST", window.location.host + "/?action=set");
    xhr.timeout = 5000;
    xhr.setRequestHeader("Content-Type", "application/json");
    xhr.send(json);
  });

  return promise;
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
          let message = "Error: {0}".format((xhr.status != 0) ? xhr.responseText : "Timeout");
          reject(message);
        }
      }
    };

    xhr.open("GET", window.location.host + "/?action=get");
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
    Status.set_error("Invalid timer setup.");
    return;
  }

  if (channelTable.getInvalidCells().length)
  {
    Status.set_error("Invalid channel setup.");
    return;
  }

  if (scheduleTable.getInvalidCells().length)
  {
    Status.set_error("Invalid schedule.");
    return;
  }

  // Copy table data into our schedule
  Schedule.data = scheduleTable.getData();

  let settings = {};
  settings.timers = Timers.dictionary;
  settings.channels = Channels.dictionary;
  settings.schedule = Schedule.dictionary;

  settings.system = {
    hostname: document.getElementById("hostname").value,
    timezone: document.getElementById("timezone").value,
    ntp_servers: [
      document.getElementById("ntp_server_1").value,
      document.getElementById("ntp_server_2").value,
    ],
  }

  Status.set("Sending settings...");
  sendJsonXhrRequest(settings).then(() => {
    Status.set_success("Complete.");
  }).catch((message) => {
    Status.set_error("Save failed.", message);
  });
}

function updateTables(settings) {
  timerTable.setData(Timers.from_dictionary(settings.timers));
  channelTable.setData(Channels.from_dictionary(settings.channels));

  // Manually trigger column update dumb!
  scheduleTable.setColumns(columnTemplate.concat(Channels.columns));
  scheduleTable.replaceData(Schedule.from_dictionary(settings.schedule));

  document.getElementById("hostname").value = settings.system.hostname;
  document.getElementById("timezone").value = settings.system.timezone;
  document.getElementById("ntp_server_1").value = settings.system.ntp_servers[0];
  document.getElementById("ntp_server_2").value = settings.system.ntp_servers[1];
}

function load() {
  Status.set("Loading settings...");
  getJsonXhrRequest().then((settings) => {
    updateTables(settings);
    Status.set_success("Settings loaded.");
  }).catch((message) => {
    Status.set_error("Load failed.", message);
  });
}

function backup() {
  Status.set("Fetching backup...");

  getJsonXhrRequest().then((settings) => {
    let element = document.createElement("a");
    element.setAttribute("href", "data:application/json;charset=utf-8," + encodeURIComponent(JSON.stringify(settings)));
    element.setAttribute("download", "{0}.json".format(settings.system.hostname));

    element.style.display = "none";
    document.body.appendChild(element);

    element.click();

    document.body.removeChild(element);

    Status.set_success("Complete.");
  }).catch((message) => {
    Status.set_error("Backup failed.", message);
  });
}

function restore() {
  let element = document.createElement("input");
  element.setAttribute("type", "file");
  element.setAttribute("accept", ".json");

  element.style.display = "none";
  document.body.appendChild(element);

  element.click();

  document.body.removeChild(element);

  element.onchange = () => {
    let file = element.files[0]
    if (!file)
      return;

    file.text().then(text => {
      updateTables(JSON.parse(text));

      Status.set_success("Settings restored.");

      let confirmed = confirm("Settings restored. Save to device now?");

      if (!confirmed)
        return;

      save();
    });
  };
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

function updateScheduleData(data) {
  // Store the data back to the Schedule object and sort it
  Schedule.data = data;
  Schedule.data.sort((f,s) => { 
    return moment(f.tod, "HH:mm") - moment(s.tod, "HH:mm");
  });
  
  // Update chart object if it exists
  if (chart)
  {
    chart.data.datasets = Schedule.datasets;
    chart.update();
  }
}
