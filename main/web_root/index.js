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
          { label: "Scale", action: scaleChannel, },
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


//
// Set/get JSON data via XHR
//

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

    xhr.open("POST", "http://" + location.host + "/?action=set");
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

    xhr.open("GET", "http://" + location.host + "/?action=get");
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


//
// Sweep generation
//

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

function generateSweep(e, column)
{
  // Set the content
  modal.setContent(sweepModal);

  // Set the onOpen function
  modal.opts.onOpen = () => {
    // Attach timepickr to our time inputs
    document.querySelectorAll(".time-picker").forEach((p) => {
      let picker = flatpickr(p, {
        enableTime: true,
        noCalendar: true,
        dateFormat: "H:i",
        defaultDate: p.value,
      });
    });

    // Restore form state
    if (window.sweepState) {
      for (let e of window.sweepState) {
        document.getElementById("modal-form").elements[e.id].value = e.value;
      }
    }
  };

  modal.opts.onClose = () => {
    // Save form state
    window.sweepState = document.getElementById("modal-form").elements;
  };
  
  modal.open_with_promise().then((formElements) => {
    // Check the channel ID of the column selected
    let id = column.getField();

    // Get start and end TODs as moment objects
    let startT = moment(formElements.startTime.value, "HH:mm");
    let endT = moment(formElements.endTime.value, "HH:mm");
    
    // Calculate the time delta in minutes, and desired step size
    let deltaT = moment.duration(endT - startT).asMinutes();
    let step = 0;
    switch (formElements.stepMode.value)
    {
      case "time":
        step = parseInt(formElements.step.value);
        break;

      case "count":
        step = deltaT / parseInt(formElements.step.value);
        break;
    }

    // Calculate the intensity delta
    let startI = parseInt(formElements.startIntensity.value);
    let endI = parseInt(formElements.endIntensity.value);
    let deltaI = endI - startI;

    // Select the interpolation function
    let interpolate = formElements.mode.value == "cubic" ? cubic_interpolate : linear_interpolate;

    let newRows = [];
    let stepT = startT;
    for (t of range(0, deltaT, step)) {
      
      let stepY = interpolate(deltaI, t/deltaT)

      row = {}
      row["tod"] = stepT.format("HH:mm");
      row[id] = Math.round(startI + stepY);

      newRows.push(row);

      stepT = stepT.add(step, "minutes")
    }

    // Add and resort table
    scheduleTable.updateOrAddData(newRows);
    scheduleTable.setSort("tod", "asc");
  }).catch(() => null);
}


function scaleChannel(e, column)
{
  // Set the content
  modal.setContent(scaleModal);

  // Nothing to do
  modal.opts.onOpen = null;
  
  modal.opts.onOpen = () => {
    // Restore form values
    if (window.scaleState) {
      for (let e of window.scaleState) {
        document.getElementById("modal-form").elements[e.id].value = e.value;
      }
    }
  };

  modal.opts.onClose = () => {
    // Save form values
    window.scaleState = document.getElementById("modal-form").elements;
  };
  
  modal.open_with_promise().then((formElements) => {
    let factor = parseFloat(formElements.ratio.value);

    column.getCells().forEach((c) => {
      if (nullOrEmpty(c.getValue()))
        return;
      let scaled = Math.round(factor * c.getValue());
      scaled = Math.min(Math.max(scaled, 0), 100);
      c.setValue(scaled);
    });
  }).catch(() => null);
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

//
// Tabulator setup
//

// Define the system config table
var timerTable = new Tabulator("#timerTable", {
  headerSort: false, // Don't allow sorting
  reactiveData: true, // Update data source as table is editted
  data: Timers.all,
  layout: "fitColumns",
  columns: [
    { title: "Timer", field: "name", widthGrow:3, },
    { title: "Frequency", field: "freq", widthGrow:2, editor: "number", 
      editorParams: {
        min: 0, 
        max: 50000, 
        step:1000, 
        mask: "99999",
      },
      validator:"max:50000",
      tooltip: (c) => !c.isValid() ? "Frequency value must be between 0 Hz and 50 kHz." : "",
    },
  ],
  validationMode: "highlight",
  tooltipGenerationMode: "hover",
});

// Define the channel table
var channelTable = new Tabulator("#channelTable", {
  headerSort: false, // Don't allow sorting
  reactiveData: true, // Update data source as table is editted
  data: Channels.all,
  layout: "fitColumns",
  columns: [
    { title: "ID", field: "id"},
    { title: "Name", field: "name", widthGrow:3, editor: "input", },
    { title: "Timer", field: "timer", widthGrow:2, editor: "select", 
      editorParams:{ 
        values:() => Timers.select_list,
        listItemFormatter:(v,t) => Timers.get_name(v),
      }, 
      formatter: (c, p, o) => Timers.get_name(c.getValue()),
    },
    { title: "GPIO", field: "gpio", editor: "number", editorParams: { min: 0, max: 31, mask: "99" }, validator: ["unique", "max:31"], 
        cellEdited: (c) => { if (!c.getData().valid) c.getRow().getCell("enabled").setValue(false) },
        tooltip: (c) => !c.isValid() ? "GPIO value must be unique and between 0-31" : "",
    },
    { title: "Enabled", field: "enabled", formatter: "tickCross", hozAlign: "center",
        cellClick: (e, c) => { if (c.getData().valid) c.setValue(!c.getValue()) },
        tooltip: (c) => !c.getData().valid ? "GPIO must be assigned to enable channel." : "",
    }
  ],
  dataChanged: () => scheduleTable.setColumns(columnTemplate.concat(Channels.columns)), // Trigger schedule column update whenever this table is changed
  validationMode: "highlight",
  tooltipGenerationMode: "hover",
});

// Template for first 2 columns of schedule table
var columnTemplate = [
  { title: "Remove", formatter: "buttonCross", hozAlign: "center", cellClick: (e, cell) => cell.getRow().delete() },
  { title: "Time Of Day", field: "tod", editor: timeEditor, headerSort: true, sorter: "time", validator: ["unique", "required"],
    cellEdited: (c) => c.getColumn().validate(),
    tooltip: (c) => !c.isValid() ? "Time Of Day must be set and unique." : "",
    sorterParams: {
      format: "HH:mm",
      alignEmptyValues: "bottom",
    }
  },
];

var scheduleTable = new Tabulator("#scheduleTable", {
  headerSort: false,
  // reactiveData doesn't appear to work if columns are variable
  //reactiveData: true,
  data: Schedule.data,
  layout:"fitDataFill",
  columns: columnTemplate,
  validationMode: "highlight",
  tooltipGenerationMode: "hover",
  index:"tod",
  dataChanged: updateScheduleData,
  dataLoaded: updateScheduleData,
});

//
// Modal setup
//

const sweepModal = `
<div>
<div class="modal-header">
<h2>Add Sweep</h2>
</div>
<form id="modal-form" action="javascript:void(0);">
  <label for="startTime">Start Time</label>
  <input class="time-picker" type="time" id="startTime" required/>

  <label for="startIntensity">Start Intensity</label>
  <input id="startIntensity" type="number" min="0" max="100" required/>

  <label for="endTime">End Time</label>
  <input class="time-picker" type="time" id="endTime" required/>

  <label for="endIntensity">End Intensity</label>
  <input id="endIntensity" type="number" min="0" max="100" required/>

  <label for="mode-group">Mode</label>
  <div class="radio-group" id="mode-group">
    <input type="radio" name="mode" id="cubic"  value="cubic" checked><label for="cubic">Cubic</label>
    <input type="radio" name="mode" id="linear" value="linear"><label for="linear">Linear</label>
  </div>
  
  <label for="step-group">Step Type</label>
  <div class="radio-group" id="step-group">
    <input type="radio" name="stepMode" id="time" value="time" onchange='document.getElementById("stepLabel").innerHTML="Minutes Per"' checked><label for="time">Time</label>
    <input type="radio" name="stepMode" id="count" value="count" onchange='document.getElementById("stepLabel").innerHTML="Step Count"'><label for="count">Count</label>
  </div>

  <label id="stepLabel" for="step">Minutes Per</label>
  <input id="step" type="number" min="1" max ="120" value="10" required/>
</form>
</div>`;


const scaleModal = `
<div>
<div class="modal-header">
<h2>Scale Channel</h2>
</div>
<form id="modal-form" action="javascript:void(0);">
  <label for="ratio">Scale Ratio</label>
  <input id="ratio" type="number" min="0" step=".01" required/>
</form>
</div>`;

// Add a function which opens the Modal and returns a promise to be resolved on close
tingle.modal.prototype.open_with_promise = function ()
{
  let promise = new Promise((resolve, reject) => {
    this.promise_resolve = resolve;
    this.promise_reject = reject;
    this.open();
  });

  return promise;
}

// Create the modal object
var modal = new tingle.modal({
  footer: true,
  closeMethods: ["overlay", "escape"],
  closeLabel: "Close",
});

// Close and resolve promise if form is OK
modal.addFooterBtn("OK", "tingle-btn tingle-btn--pull-right", () => {
  if (document.getElementById("modal-form").reportValidity())
  {
    modal.promise_resolve(document.getElementById("modal-form").elements);
    modal.close();
  }
});

// Close and reject promise
modal.addFooterBtn("Cancel", "tingle-btn tingle-btn--pull-right", () => {
  modal.promise_reject();
  modal.close();
});

//
// Chart setup
//
var context = document.getElementById("scheduleChart").getContext("2d");
var chart = new Chart(context, {
  type: "line",
  options: {
    responsive: true,
    maintainAspectRatio: false,
    datasets: {
      line: {
        steppedLine: "before",
        fill: false,
      },
    },
    scales: {
      xAxes: [{
        type: "time",
        time: {
          parser: "HH:mm",
          minUnit: "hour",
        },
        ticks: {
          min: "00:00",
          max: "24:00",
        },
      }],
      yAxes: [{
        ticks: {
          min: 0,
          max: 100,
        },
      }],
    },
    tooltips: {
      position: "nearest",
      mode: "index",
    },
    plugins: {
      colorschemes: {
        scheme: "brewer.Accent8"
      }
    }
  }
});

let WSTimeout = {
  _timeout: null,
  _action: null,
  _interval: null,

  reset: function () {
    if (this._timeout)
      clearTimeout(this._timeout);
    
    if (this._interval && this._action)
      this._timeout = setTimeout(this._action, this._interval);
  },

  start: function (action, interval) {
    this._action = action;
    this._interval = interval;

    return this.reset();
  },
}
WSTimeout.start(()=> document.getElementById("system_time").innerHTML = "__:__:__ __", 1000);

// Start status connection
let socket = new WebSocket("ws://" + location.host);
socket.onmessage = (ev) => {
  let status = JSON.parse(ev.data);
  let now = moment.parseZone(status.time);
  if (now.isValid)
  {
    WSTimeout.reset();

    document.getElementById("system_time").innerHTML = now.format("h:mm:ss A");
  }
};

// Fetch data from remote
window.onload = () => load();