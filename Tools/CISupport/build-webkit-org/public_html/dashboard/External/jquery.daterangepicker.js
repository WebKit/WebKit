// daterangepicker.js
// version : 0.0.5
// author : Chunlong Liu
// last updated at: 2014-05-27
// license : MIT
// www.jszen.com

(function($)
{

    $.dateRangePickerLanguages =
    {
        'cn':
        {
            'selected': '已选择:',
            'day':'天',
            'days': '天',
            'apply': '确定',
            'week-1' : '一',
            'week-2' : '二',
            'week-3' : '三',
            'week-4' : '四',
            'week-5' : '五',
            'week-6' : '六',
            'week-7' : '日',
            'month-name': ['一月','二月','三月','四月','五月','六月','七月','八月','九月','十月','十一月','十二月'],
            'shortcuts' : '快捷选择',
            'past': '过去',
            'following':'将来',
            'previous' : '&nbsp;&nbsp;&nbsp;',
            'prev-week' : '上周',
            'prev-month' : '上个月',
            'prev-year' : '去年',
            'next': '&nbsp;&nbsp;&nbsp;',
            'next-week':'下周',
            'next-month':'下个月',
            'next-year':'明年',
            'less-than' : '所选日期范围不能大于%d天',
            'more-than' : '所选日期范围不能小于%d天',
            'default-more' : '请选择大于%d天的日期范围',
            'default-less' : '请选择小于%d天的日期范围',
            'default-range' : '请选择%d天到%d天的日期范围',
            'default-single':'请选择一个日期',
            'default-default': '请选择一个日期范围'
        },
        'en':
        {
            'selected': 'Selected:',
            'day':'Day',
            'days': 'Days',
            'apply': 'Start',
            'week-1' : 'MO',
            'week-2' : 'TU',
            'week-3' : 'WE',
            'week-4' : 'TH',
            'week-5' : 'FR',
            'week-6' : 'SA',
            'week-7' : 'SU',
            'month-name': ['JANUARY','FEBRUARY','MARCH','APRIL','MAY','JUNE','JULY','AUGUST','SEPTEMBER','OCTOBER','NOVEMBER','DECEMBER'],
            'shortcuts' : 'Shortcuts',
            'past': 'Past',
            'following':'Following',
            'previous' : 'Previous',
            'prev-week' : 'Week',
            'prev-month' : 'Month',
            'prev-year' : 'Year',
            'next':'Next',
            'next-week':'Week',
            'next-month':'Month',
            'next-year':'Year',
            'less-than' : 'Date range should not be more than %d days',
            'more-than' : 'Date range should not be less than %d days',
            'default-more' : 'Please select a date range longer than %d days',
            'default-single' : 'Please select a date',
            'default-less' : 'Please select a date range less than %d days',
            'default-range' : 'Please select a date range between %d and %d days',
            'default-default': 'Please select a date range'
        },
        'it':
        {
            'selected': 'Selezionati:',
            'day':'Giorno',
            'days': 'Giorni',
            'apply': 'Chiudi',
            'week-1' : 'LU',
            'week-2' : 'MA',
            'week-3' : 'ME',
            'week-4' : 'GI',
            'week-5' : 'VE',
            'week-6' : 'SA',
            'week-7' : 'DO',
            'month-name': ['GENNAIO','FEBBRAIO','MARZO','APRILE','MAGGIO','GIUGNO','LUGLIO','AGOSTO','SETTEMBRE','OTTOBRE','NOVEMBRE','DICEMBRE'],
            'shortcuts' : 'Scorciatoie',
            'past': 'Scorso',
            'following':'Successivo',
            'previous' : 'Precedente',
            'prev-week' : 'Settimana',
            'prev-month' : 'Mese',
            'prev-year' : 'Anno',
            'next':'Prossimo',
            'next-week':'Settimana',
            'next-month':'Mese',
            'next-year':'Anno',
            'less-than' : 'L\'intervallo non dev\'essere maggiore di %d giorni',
            'more-than' : 'L\'intervallo non dev\'essere minore di %d giorni',
            'default-more' : 'Seleziona un intervallo maggiore di %d giorni',
            'default-single' : 'Seleziona una data',
            'default-less' : 'Seleziona un intervallo minore di %d giorni',
            'default-range' : 'Seleziona un intervallo compreso tra i %d e i %d giorni',
            'default-default': 'Seleziona un intervallo di date'
        },
        'es':
        {
            'selected': 'Seleccionado:',
            'day':'Dia',
            'days': 'Dias',
            'apply': 'Cerrar',
            'week-1' : 'LU',
            'week-2' : 'MA',
            'week-3' : 'MI',
            'week-4' : 'JU',
            'week-5' : 'VI',
            'week-6' : 'SA',
            'week-7' : 'DO',
            'month-name': ['ENERO','FEBRERO','MARZO','ABRIL','MAYO','JUNIO','JULIO','AGOSTO','SEPTIEMBRE','OCTUBRE','NOVIEMBRE','DICIEMBRE'],
            'shortcuts' : 'Accesos directos',
            'past': 'Pasado',
            'following':'Siguiente',
            'previous' : 'Anterior',
            'prev-week' : 'Semana',
            'prev-month' : 'Mes',
            'prev-year' : 'Año',
            'next':'Siguiente',
            'next-week':'Semana',
            'next-month':'Mes',
            'next-year':'Año',
            'less-than' : 'El rango no deberia ser mayor de %d dias',
            'more-than' : 'El rango no deberia ser menor de %d dias',
            'default-more' : 'Por favor selecciona un rango mayor a %d dias',
            'default-single' : 'Por favor selecciona un dia',
            'default-less' : 'Por favor selecciona un rango menor a %d dias',
            'default-range' : 'Por favor selecciona un rango entre %d y %d dias',
            'default-default': 'Por favor selecciona un rango de fechas.'
        },
        'de':
        {
            'selected': 'Auswahl:',
            'day':'Tag',
            'days': 'Tage',
            'apply': 'Schließen',
            'week-1' : 'MO',
            'week-2' : 'DI',
            'week-3' : 'MI',
            'week-4' : 'DO',
            'week-5' : 'FR',
            'week-6' : 'SA',
            'week-7' : 'SO',
            'month-name': ['JANUAR','FEBRUAR','MÄRZ','APRIL','MAI','JUNI','JULI','AUGUST','SEPTEMBER','OKTOBER','NOVEMBER','DEZEMBER'],
            'shortcuts' : 'Schnellwahl',
            'past': 'Vorherige',
            'following':'Folgende',
            'previous' : 'Vorherige',
            'prev-week' : 'Woche',
            'prev-month' : 'Monat',
            'prev-year' : 'Jahr',
            'next':'Nächste',
            'next-week':'Woche',
            'next-month':'Monat',
            'next-year':'Jahr',
            'less-than' : 'Datumsbereich darf nicht größer sein als %d Tage',
            'more-than' : 'Datumsbereich darf nicht kleiner sein als %d Tage',
            'default-more' : 'Bitte mindestens %d Tage auswählen',
            'default-single' : 'Bitte ein Datum auswählen',
            'default-less' : 'Bitte weniger als %d Tage auswählen',
            'default-range' : 'Bitte einen Datumsbereich zwischen %d und %d Tagen auswählen',
            'default-default': 'Bitte ein Start- und Enddatum auswählen'
        }
    };


    if (window['moment'] === undefined)
    {
        if (window['console'] && console['warn']) console.warn('Please import moment.js before daterangepicker.js');
        return;
    }

    $.fn.dateRangePicker = function(opt)
    {
        if (!opt) opt = {};
        opt = $.extend(true,
        {
            autoClose: false,
            format: 'YYYY-MM-DD',
            separator: ' to ',
            language: 'auto',
            startOfWeek: 'sunday',// or monday
            getValue: function()
            {
                return $(this).val();
            },
            setValue: function(s)
            {
                $(this).val(s);
            },
            startDate: false,
            endDate: false,
            time: {
                enabled: false
            },
            minDays: 0,
            maxDays: 0,
            showShortcuts: true,
            shortcuts:
            {
                //'prev-days': [1,3,5,7],
                'next-days': [3,5,7],
                //'prev' : ['week','month','year'],
                'next' : ['week','month','year']
            },
            customShortcuts : [],
            inline:false,
            container:'body',
            alwaysOpen:false,
            singleDate:false,
            batchMode: false
        },opt);

        opt.start = false;
        opt.end = false;

        if (opt.startDate && typeof opt.startDate == 'string') opt.startDate = moment(opt.startDate,opt.format).toDate();
        if (opt.endDate && typeof opt.endDate == 'string') opt.endDate = moment(opt.endDate,opt.format).toDate();

        var langs = getLanguages();
        var box;
        var initiated = false;
        var self = this;
        var selfDom = $(self).get(0);

        $(this).unbind('.datepicker').bind('click.datepicker',function(evt)
        {
            evt.stopPropagation();
            open(200);
        });

        init_datepicker.call(this);

        if (opt.alwaysOpen)
        {
            open(0);
        }

        // expose some api
        $(this).data('dateRangePicker',
        {
            setDateRange : function(d1,d2)
            {
                if (typeof d1 == 'string' && typeof d2 == 'string')
                {
                    d1 = moment(d1,opt.format).toDate();
                    d2 = moment(d2,opt.format).toDate();
                }
                setDateRange(d1,d2);
            },
            clear: clearSelection,
            close: closeDatePicker,
            open: open,
            destroy: function()
            {
                $(self).unbind('.datepicker');
                $(self).data('dateRangePicker','');
                box.remove();
                $(window).unbind('resize.datepicker',calcPosition);
                $(document).unbind('click.datepicker',closeDatePicker);
            }
        });

        $(window).bind('resize.datepicker',calcPosition);

        return this;











        function init_datepicker()
        {
            var self = this;

            if ($(this).data('date-picker-opened'))
            {
                closeDatePicker();
                return;
            }
            $(this).data('date-picker-opened',true);





            box = createDom().hide();
            $(opt.container).append(box);

            if (!opt.inline)
            {
                calcPosition();
            }
            else
            {
                box.css({position:'static'});
            }

            if (opt.alwaysOpen)
            {
                box.find('.apply-btn').hide();
            }

            var defaultTime = new Date();
            if (opt.startDate && compare_month(defaultTime,opt.startDate) < 0 ) defaultTime = moment(opt.startDate).toDate();
            if (opt.endDate && compare_month(nextMonth(defaultTime),opt.endDate) > 0 ) defaultTime = prevMonth(moment(opt.endDate).toDate());


            showMonth(defaultTime,'month1');
            showMonth(nextMonth(defaultTime),'month2');

            if (opt.time.enabled) {
                if ((opt.startDate && opt.endDate) || (opt.start && opt.end)) {
                    showTime(moment(opt.start || opt.startDate).toDate(),'time1');
                    showTime(moment(opt.end || opt.endDate).toDate(),'time2');
                } else {
                    showTime(defaultTime,'time1');
                    showTime(defaultTime,'time2');
                }
            }

            //showSelectedInfo();




            var defaultTopText = '';
            if (opt.singleDate)
                defaultTopText = lang('default-single');
            else if (opt.minDays && opt.maxDays)
                defaultTopText = lang('default-range');
            else if (opt.minDays)
                defaultTopText = lang('default-more');
            else if (opt.maxDays)
                defaultTopText = lang('default-less');
            else
                defaultTopText = lang('default-default');

            box.find('.default-top').html( defaultTopText.replace(/\%d/,opt.minDays).replace(/\%d/,opt.maxDays));




            setTimeout(function()
            {
                initiated = true;
            },0);

            box.click(function(evt)
            {
                evt.stopPropagation();
            });

            $(document).bind('click.datepicker',closeDatePicker);

            box.find('.next').click(function()
            {
                var isMonth2 = $(this).parents('table').hasClass('month2');
                var month = isMonth2 ? opt.month2 : opt.month1;
                month = nextMonth(month);
                if (!opt.singleDate && !isMonth2 && compare_month(month,opt.month2) >= 0) return;
                showMonth(month,isMonth2 ? 'month2' : 'month1');
                showGap();
            });

            box.find('.prev').click(function()
            {
                var isMonth2 = $(this).parents('table').hasClass('month2');
                var month = isMonth2 ? opt.month2 : opt.month1;
                month = prevMonth(month);
                //if (isMonth2 && month.getFullYear()+''+month.getMonth() <= opt.month1.getFullYear()+''+opt.month1.getMonth()) return;
                if (isMonth2 && compare_month(month,opt.month1) <= 0) return;
                showMonth(month,isMonth2 ? 'month2' : 'month1');
                showGap();
            });


            box.bind('click',function(evt)
            {
                if ($(evt.target).hasClass('day'))
                {
                    dayClicked($(evt.target));
                }
            });

            box.attr('unselectable', 'on')
            .css('user-select', 'none')
            .bind('selectstart', function(e)
            {
                e.preventDefault(); return false;
            });

            box.find('.apply-btn').click(function()
            {
                closeDatePicker();
                var dateRange = getDateString(new Date(opt.start))+ opt.separator +getDateString(new Date(opt.end));
                $(self).trigger('datepicker-apply',
                {
                    'value': dateRange,
                    'date1' : new Date(opt.start),
                    'date2' : new Date(opt.end)
                });
            });

            box.find('[shortcut]').click(function()
            {
                var shortcut = $(this).attr('shortcut');
                var end = new Date(),start = false;
                if (shortcut.indexOf('day') != -1)
                {
                    var day = parseInt(shortcut.split(',',2)[1],10);
                    start = new Date(new Date().getTime() + 86400000*day);
                    end = new Date(end.getTime() + 86400000*(day>0?1:-1) );
                }
                else if (shortcut.indexOf('week')!= -1)
                {
                    var dir = shortcut.indexOf('prev,') != -1 ? -1 : 1;

                    if (dir == 1)
                        var stopDay = opt.startOfWeek == 'monday' ? 1 : 0;
                    else
                        var stopDay = opt.startOfWeek == 'monday' ? 0 : 6;

                    end = new Date(end.getTime() - 86400000);
                    while(end.getDay() != stopDay) end = new Date(end.getTime() + dir*86400000);
                    start = new Date(end.getTime() + dir*86400000*6);
                }
                else if (shortcut.indexOf('month') != -1)
                {
                    var dir = shortcut.indexOf('prev,') != -1 ? -1 : 1;
                    if (dir == 1)
                        start = nextMonth(end);
                    else
                        start = prevMonth(end);
                    start.setDate(1);
                    end = nextMonth(start);
                    end.setDate(1);
                    end = new Date(end.getTime() - 86400000);
                }
                else if (shortcut.indexOf('year') != -1)
                {
                    var dir = shortcut.indexOf('prev,') != -1 ? -1 : 1;
                    start = new Date();
                    start.setFullYear(end.getFullYear() + dir);
                    start.setMonth(0);
                    start.setDate(1);
                    end.setFullYear(end.getFullYear() + dir);
                    end.setMonth(11);
                    end.setDate(31);
                }
                else if (shortcut == 'custom')
                {
                    var name = $(this).html();
                    if (opt.customShortcuts && opt.customShortcuts.length > 0)
                    {
                        for(var i=0;i<opt.customShortcuts.length;i++)
                        {
                            var sh = opt.customShortcuts[i];
                            if (sh.name == name)
                            {
                                var data = [];
                                // try
                                // {
                                    data = sh['dates'].call();
                                //}catch(e){}
                                if (data && data.length == 2)
                                {
                                    start = data[0];
                                    end = data[1];
                                }

                                // if only one date is specified then just move calendars there
                                // move calendars to show this date's month and next months
                                if (data && data.length == 1)
                                {
                                    movetodate = data[0];
                                    showMonth(movetodate,'month1');
                                    showMonth(nextMonth(movetodate),'month2');
                                    showGap();
                                }

                                break;
                            }
                        }
                    }
                }
                if (start && end)
                {
                    setDateRange(start,end);
                    checkSelectionValid();
                }
            });

            box.find(".time1 input[type=range]").bind("change mousemove", function (e) {
                var target = e.target,
                    hour = target.name == "hour" ? $(target).val().replace(/^(\d{1})$/, "0$1") : undefined,
                    min = target.name == "minute" ? $(target).val().replace(/^(\d{1})$/, "0$1") : undefined;
                setTime("time1", hour, min);
            });

            box.find(".time2 input[type=range]").bind("change mousemove", function (e) {
                var target = e.target,
                    hour = target.name == "hour" ? $(target).val().replace(/^(\d{1})$/, "0$1") : undefined,
                    min = target.name == "minute" ? $(target).val().replace(/^(\d{1})$/, "0$1") : undefined;
                setTime("time2", hour, min);
            });

        }


        function calcPosition()
        {
            if (!opt.inline)
            {
                var offset = $(self).offset();
/* Apple change
                if (offset.left < 460) //left to right
                {
                    box.css(
                    {
                        top: offset.top+$(self).outerHeight() + parseInt($('body').css('border-top') || 0,10 ),
                        left: offset.left
                    });
                }
                else
                {
                    box.css(
                    {
                        top: offset.top+$(self).outerHeight() + parseInt($('body').css('border-top') || 0,10 ),
                        left: offset.left + $(self).width() - box.width() - 16
                    });
                }
*/
                box.css(
                {
                    top: $(self).outerHeight() + parseInt($('body').css('border-top') || 0,10 ),
//                    left: offset.left + $(self).width() - box.width() - 16
                });
            }
        }

        function open(animationTime)
        {
            calcPosition();
            var __default_string = opt.getValue.call(selfDom);
            var defaults = __default_string ? __default_string.split( opt.separator ) : '';

            if (defaults && defaults.length >= 2)
            {
                var ___format = opt.format;
                if (___format.match(/Do/))
                {
                    ___format = ___format.replace(/Do/,'D');
                    defaults[0] = defaults[0].replace(/(\d+)(th|nd|st)/,'$1');
                    defaults[1] = defaults[1].replace(/(\d+)(th|nd|st)/,'$1');
                }
        // set initiated  to avoid triggerring datepicker-change event
        initiated = false;
                setDateRange(moment(defaults[0], ___format).toDate(),moment(defaults[1], ___format).toDate());
        initiated = true;
            }
            box.slideDown(animationTime);
        }



        function renderTime (name, date) {
            box.find("." + name + " input[type=range].hour-range").val(moment(date).hours());
            box.find("." + name + " input[type=range].minute-range").val(moment(date).minutes());
            setTime(name, moment(date).format("HH"), moment(date).format("mm"));
        }

        function changeTime (name, date) {
            opt[name] = parseInt(
                moment(parseInt(date))
                    .startOf('day')
                    .add('h', moment(opt[name + "Time"]).format("HH"))
                    .add('m', moment(opt[name + "Time"]).format("mm")).valueOf()
                );
        }

        function swapTime () {
            renderTime("time1", opt.start);
            renderTime("time2", opt.end);
        }

        function setTime (name, hour, minute) {
            hour && (box.find("." + name + " .hour-val").text(hour));
            minute && (box.find("." + name + " .minute-val").text(minute));
            switch (name) {
                case "time1":
                    if (opt.start) {
                        setRange("start", moment(opt.start));
                    }
                    setRange("startTime", moment(opt.startTime || moment().valueOf()));
                    break;
                case "time2":
                    if (opt.end) {
                        setRange("end", moment(opt.end));
                    }
                    setRange("endTime", moment(opt.endTime || moment().valueOf()));
                    break;
            }
            function setRange(name, timePoint) {
                var h = timePoint.format("HH"),
                    m = timePoint.format("mm");
                opt[name] = timePoint
                    .startOf('day')
                    .add("h", hour || h)
                    .add("m", minute || m)
                    .valueOf();
            }
            checkSelectionValid();
            showSelectedInfo();
            showSelectedDays();
        }

        function clearSelection()
        {
            opt.start = false;
            opt.end = false;
            box.find('.day.checked').removeClass('checked');
            opt.setValue.call(selfDom, '');
            checkSelectionValid();
            showSelectedInfo();
            showSelectedDays();
        }

    function handleStart(time)
    {
      var r = time;
      if  (opt.batchMode === 'week-range') {
        if (opt.startOfWeek === 'monday') {
                    r = moment(parseInt(time)).startOf('isoweek').valueOf();
                } else {
                    r = moment(parseInt(time)).startOf('week').valueOf();
                }
      } else if (opt.batchMode === 'month-range') {
                r = moment(parseInt(time)).startOf('month').valueOf();
      }

      return r;
    }

    function handleEnd(time)
    {
      var r = time;
      if  (opt.batchMode === 'week-range') {
        if (opt.startOfWeek === 'monday-range') {
                    r = moment(parseInt(time)).endOf('isoweek').valueOf();
                } else {
                    r = moment(parseInt(time)).endOf('week').valueOf();
                }
      } else if (opt.batchMode === 'month') {
                r = moment(parseInt(time)).endOf('month').valueOf();
      }

      return r;
    }


        function dayClicked(day)
        {
            if (day.hasClass('invalid')) return;
            var time = day.attr('time');
            day.addClass('checked');
            if ( opt.singleDate )
            {
                opt.start = time;
                opt.end = false;
                if (opt.time.enabled) {
                    changeTime("start", opt.start);
                }
            }
            else if  (opt.batchMode === 'week')
            {
                if (opt.startOfWeek === 'monday') {
                    opt.start = moment(parseInt(time)).startOf('isoweek').valueOf();
                    opt.end = moment(parseInt(time)).endOf('isoweek').valueOf();
                } else {
                    opt.end = moment(parseInt(time)).endOf('week').valueOf();
                    opt.start = moment(parseInt(time)).startOf('week').valueOf();
                }
            }
            else if (opt.batchMode === 'month')
            {
                opt.start = moment(parseInt(time)).startOf('month').valueOf();
                opt.end = moment(parseInt(time)).endOf('month').valueOf();
            }
            else if ((opt.start && opt.end) || (!opt.start && !opt.end) )
            {
                opt.start = handleStart(time);
                opt.end = false;
                if (opt.time.enabled) {
                    changeTime("start", opt.start);
                }
            }
            else if (opt.start)
            {
                opt.end = handleEnd(time);
                if (opt.time.enabled) {
                    changeTime("end", opt.end);
                }
            }

            if (!opt.singleDate && opt.start && opt.end && opt.start > opt.end)
            {
                var tmp = opt.end;
                opt.end = handleEnd(opt.start);
                opt.start = handleStart(tmp);
                if (opt.time.enabled) {
                    swapTime();
                }
            }

            opt.start = parseInt(opt.start);
            opt.end = parseInt(opt.end);

            checkSelectionValid();
            showSelectedInfo();
            showSelectedDays();
            autoclose();
        }

        function autoclose () {
            if (opt.singleDate === true) {
                if (initiated && opt.start )
                {
                    if (opt.autoClose) closeDatePicker();
                }
            } else {
                if (initiated && opt.start && opt.end)
                {
                    if (opt.autoClose) closeDatePicker();
                }
            }
        }

        function checkSelectionValid()
        {
            var days = Math.ceil( (opt.end - opt.start) / 86400000 ) + 1;
            if (opt.singleDate) { // Validate if only start is there
                if (opt.start && !opt.end)
                    box.find('.drp_top-bar').removeClass('error').addClass('normal');
                else
                    box.find('.drp_top-bar').removeClass('error').removeClass('normal');
            }
            else if ( opt.maxDays && days > opt.maxDays)
            {
                opt.start = false;
                opt.end = false;
                box.find('.day').removeClass('checked');
                box.find('.drp_top-bar').removeClass('normal').addClass('error').find('.error-top').html( lang('less-than').replace('%d',opt.maxDays) );
            }
            else if ( opt.minDays && days < opt.minDays)
            {
                opt.start = false;
                opt.end = false;
                box.find('.day').removeClass('checked');
                box.find('.drp_top-bar').removeClass('normal').addClass('error').find('.error-top').html( lang('more-than').replace('%d',opt.minDays) );
            }
            else
            {
                if (opt.start || opt.end)
                    box.find('.drp_top-bar').removeClass('error').addClass('normal');
                else
                    box.find('.drp_top-bar').removeClass('error').removeClass('normal');
            }

            if ( (opt.singleDate && opt.start && !opt.end) || (!opt.singleDate && opt.start && opt.end) )
            {
                box.find('.apply-btn').removeClass('disabled');
            }
            else
            {
                box.find('.apply-btn').addClass('disabled');
            }

            if (opt.batchMode)
            {
                if ( (opt.start && opt.startDate && compare_day(opt.start, opt.startDate) < 0)
                    || (opt.end && opt.endDate && compare_day(opt.end, opt.endDate) > 0)  )
                {
                    opt.start = false;
                    opt.end = false;
                    box.find('.day').removeClass('checked');
                }
            }
        }

        function showSelectedInfo()
        {
            box.find('.start-day').html('...');
            box.find('.end-day').html('...');
            box.find('.selected-days').hide();
            if (opt.start)
            {
                box.find('.start-day').html(getDateString(new Date(parseInt(opt.start))));
            }
            if (opt.end)
            {
                box.find('.end-day').html(getDateString(new Date(parseInt(opt.end))));
            }

            if (opt.start && opt.singleDate)
            {
                box.find('.apply-btn').removeClass('disabled');
                var dateRange = getDateString(new Date(opt.start));
                opt.setValue.call(selfDom, dateRange, getDateString(new Date(opt.start)), getDateString(new Date(opt.end)));

                if (initiated)
                {
                    $(self).trigger('datepicker-change',
                    {
                        'value': dateRange,
                        'date1' : new Date(opt.start)
                    });
                }
            }
            else if (opt.start && opt.end)
            {
                box.find('.selected-days').show().find('.selected-days-num').html(Math.round((opt.end-opt.start)/86400000)+1);
                box.find('.apply-btn').removeClass('disabled');
                var dateRange = getDateString(new Date(opt.start))+ opt.separator +getDateString(new Date(opt.end));
                opt.setValue.call(selfDom,dateRange, getDateString(new Date(opt.start)), getDateString(new Date(opt.end)));
                if (initiated)
                {
                    $(self).trigger('datepicker-change',
                    {
                        'value': dateRange,
                        'date1' : new Date(opt.start),
                        'date2' : new Date(opt.end)
                    });
                }
            }
            else
            {
                box.find('.apply-btn').addClass('disabled');
            }
        }

        function setDateRange(date1,date2)
        {
            if (date1.getTime() > date2.getTime())
            {
                var tmp = date2;
                date2 = date1;
                date1 = tmp;
                tmp = null;
            }
            var valid = true;
            if (opt.startDate && compare_day(date1,opt.startDate) < 0) valid = false;
            if (opt.endDate && compare_day(date2,opt.endDate) > 0) valid = false;
            if (!valid)
            {
                showMonth(opt.startDate,'month1');
                showMonth(nextMonth(opt.startDate),'month2');
                showGap();
                return;
            }

            opt.start = date1.getTime();
            opt.end = date2.getTime();
            if (compare_month(date1,date2) == 0)
            {
                date2 = nextMonth(date1);
            }
            if (opt.time.enabled) {
                renderTime("time1", date1);
                renderTime("time2", date2);
            }
            showMonth(date1,'month1');
            showMonth(date2,'month2');
            showGap();
            showSelectedInfo();
            autoclose();
        }

        function showSelectedDays()
        {
            if (!opt.start && !opt.end) return;
            box.find('.day').each(function()
            {
                var time = parseInt($(this).attr('time')),
                    start = opt.start,
                    end = opt.end;
                if (opt.time.enabled) {
                    time = moment(time).startOf('day').valueOf();
                    start = moment(start || moment().valueOf()).startOf('day').valueOf();
                    end = moment(end || moment().valueOf()).startOf('day').valueOf();
                }
                if (
                    (opt.start && opt.end && end >= time && start <= time )
                    || ( opt.start && !opt.end && moment(start).format('YYYY-MM-DD') == moment(time).format('YYYY-MM-DD') )
                )
                {
                    $(this).addClass('checked');
                }
                else
                {
                    $(this).removeClass('checked');
                }
            });
        }

        function showMonth(date,month)
        {
            date = moment(date).startOf('day').toDate();
            var monthName = nameMonth(date.getMonth());
            box.find('.'+month+' .month-name').html(monthName+' '+date.getFullYear());
            box.find('.'+month+' tbody').html(createMonthHTML(date));
            opt[month] = date;
        }

        function showTime(date,name)
        {
            box.find('.' + name).append(getTimeHTML());
            renderTime(name, date);
        }

        function nameMonth(m)
        {
            return lang('month-name')[m];
        }

        function getDateString(d)
        {
            return moment(d).format(opt.format);
        }

        function showGap()
        {
            showSelectedDays();
            var m1 = parseInt(moment(opt.month1).format('YYYYMM'));
            var m2 = parseInt(moment(opt.month2).format('YYYYMM'));
            var p = Math.abs(m1 - m2);
            var shouldShow = (p > 1 && p !=89);
            if (shouldShow)
                box.find('.gap').show();
            else
                box.find('.gap').hide();
        }

        function closeDatePicker()
        {
            if (opt.alwaysOpen) return;
            $(box).slideUp(200,function()
            {
                $(self).data('date-picker-opened',false);
            });
            //$(document).unbind('.datepicker');
            $(self).trigger('datepicker-close');
        }

        function compare_month(m1,m2)
        {
            var p = parseInt(moment(m1).format('YYYYMM')) - parseInt(moment(m2).format('YYYYMM'));
            if (p > 0 ) return 1;
            if (p == 0) return 0;
            return -1;
        }

        function compare_day(m1,m2)
        {
            var p = parseInt(moment(m1).format('YYYYMMDD')) - parseInt(moment(m2).format('YYYYMMDD'));
            if (p > 0 ) return 1;
            if (p == 0) return 0;
            return -1;
        }

        function nextMonth(month)
        {
            month = moment(month).toDate();
            var toMonth = month.getMonth();
            while(month.getMonth() == toMonth) month = new Date(month.getTime()+86400000);
            return month;
        }

        function prevMonth(month)
        {
            month = moment(month).toDate();
            var toMonth = month.getMonth();
            while(month.getMonth() == toMonth) month = new Date(month.getTime()-86400000);
            return month;
        }

        function getTimeHTML()
        {
            var timeHtml = '<div>'
                +'<span>Time: <span class="hour-val">00</span>:<span class="minute-val">00</span></span>'
                +'</div>'
                +'<div class="hour">'
                +'<label>Hour: <input type="range" class="hour-range" name="hour" min="0" max="23"></label>'
                +'</div>'
                +'<div class="minute">'
                +'<label>Minute: <input type="range" class="minute-range" name="minute" min="0" max="59"></label>'
                +'</div>';
            return timeHtml;
        }

        function createDom()
        {
            var html = '<div class="date-picker-wrapper'
            if ( opt.singleDate ) html += ' single-date';
            if ( !opt.showShortcuts ) html += ' no-shortcuts ';
            html += '">'
                +'<div class="drp_top-bar">\
                    <div class="normal-top">\
                        <span>'+lang('selected')+' </span> <b class="start-day">...</b>'
            if ( ! opt.singleDate ) {
                html += ' <span class="separator-day">'+opt.separator+'</span> <b class="end-day">...</b> <i class="selected-days">(<span class="selected-days-num">3</span> '+lang('days')+')</i>'
            }
            html += '</div>\
                    <div class="error-top">error</div>\
                    <div class="default-top">default</div>\
                    <input type="button" class="apply-btn disabled '+ getHideClass() +'" value="'+lang('apply')+'" />\
                </div>'
                +'<div class="month-wrapper">'
                +'<table class="month1" cellspacing="0" border="0" cellpadding="0"><thead><tr class="caption"><th style="width:27px;"><span class="prev">&lt;</span></th><th colspan="5" class="month-name">January, 2011</th><th style="width:27px;"><span class="next">&gt;</span></th></tr><tr class="week-name">'+getWeekHead()+'</thead><tbody></tbody></table>'
            if ( ! opt.singleDate ) {
                html += '<div class="gap">'+getGapHTML()+'</div>'
                    +'<table class="month2" cellspacing="0" border="0" cellpadding="0"><thead><tr class="caption"><th style="width:27px;"><span class="prev">&lt;</span></th><th colspan="5" class="month-name">January, 2011</th><th style="width:27px;"><span class="next">&gt;</span></th></tr><tr class="week-name">'+getWeekHead()+'</thead><tbody></tbody></table>'
            }
                //+'</div>'
            html +=    '<div style="clear:both;height:0;font-size:0;"></div>'
                +'<div class="time">'
                +'<div class="time1"></div>'
            if ( ! opt.singleDate ) {
                html += '<div class="time2"></div>'
            }
            html += '</div>'
                +'<div style="clear:both;height:0;font-size:0;"></div>'
                +'</div>';

            if (opt.showShortcuts)
            {
                html += '<div class="footer"><b>'+lang('shortcuts')+'</b>';

                var data = opt.shortcuts;
                if (data)
                {
                    if (data['prev-days'] && data['prev-days'].length > 0)
                    {
                        html += '&nbsp;<span class="prev-days">'+lang('past');
                        for(var i=0;i<data['prev-days'].length; i++)
                        {
                            var name = data['prev-days'][i];
                            name += (data['prev-days'][i] > 1) ? lang('days') : lang('day');
                            html += ' <a href="javascript:;" shortcut="day,-'+data['prev-days'][i]+'">'+name+'</a>';
                        }
                        html+='</span>';
                    }

                    if (data['next-days'] && data['next-days'].length > 0)
                    {
                        html += '&nbsp;<span class="next-days">'+lang('following');
                        for(var i=0;i<data['next-days'].length; i++)
                        {
                            var name = data['next-days'][i];
                            name += (data['next-days'][i] > 1) ? lang('days') : lang('day');
                            html += ' <a href="javascript:;" shortcut="day,'+data['next-days'][i]+'">'+name+'</a>';
                        }
                        html+= '</span>';
                    }

                    if (data['prev'] && data['prev'].length > 0)
                    {
                        html += '&nbsp;<span class="prev-buttons">'+lang('previous');
                        for(var i=0;i<data['prev'].length; i++)
                        {
                            var name = lang('prev-'+data['prev'][i]);
                            html += ' <a href="javascript:;" shortcut="prev,'+data['prev'][i]+'">'+name+'</a>';
                        }
                        html+='</span>';
                    }

                    if (data['next'] && data['next'].length > 0)
                    {
                        html += '&nbsp;<span class="next-buttons">'+lang('next');
                        for(var i=0;i<data['next'].length; i++)
                        {
                            var name = lang('next-'+data['next'][i]);
                            html += ' <a href="javascript:;" shortcut="next,'+data['next'][i]+'">'+name+'</a>';
                        }
                        html+='</span>';
                    }
                }

                if (opt.customShortcuts)
                {
                    for(var i=0;i<opt.customShortcuts.length; i++)
                    {
                        var sh = opt.customShortcuts[i];
                        html+= '&nbsp;<span class="custom-shortcut"><a href="javascript:;" shortcut="custom">'+sh.name+'</a></span>';
                    }
                }

                html +='</div>';
            }
            html += '</div>';


            return $(html);
        }

        function getHideClass()
        {
            if (opt.autoClose === true) {
                return 'hide';
            }
            return '';
        }

        function getWeekHead()
        {
            if (opt.startOfWeek == 'monday')
            {
                return '<th>'+lang('week-1')+'</th>\
                    <th>'+lang('week-2')+'</th>\
                    <th>'+lang('week-3')+'</th>\
                    <th>'+lang('week-4')+'</th>\
                    <th>'+lang('week-5')+'</th>\
                    <th>'+lang('week-6')+'</th>\
                    <th>'+lang('week-7')+'</th>';
            }
            else
            {
                return '<th>'+lang('week-7')+'</th>\
                    <th>'+lang('week-1')+'</th>\
                    <th>'+lang('week-2')+'</th>\
                    <th>'+lang('week-3')+'</th>\
                    <th>'+lang('week-4')+'</th>\
                    <th>'+lang('week-5')+'</th>\
                    <th>'+lang('week-6')+'</th>';
            }
        }

        function getGapHTML()
        {
            var html = ['<div class="gap-top-mask"></div><div class="gap-bottom-mask"></div><div class="gap-lines">'];
            for(var i=0;i<20;i++)
            {
                html.push('<div class="gap-line">\
                    <div class="gap-1"></div>\
                    <div class="gap-2"></div>\
                    <div class="gap-3"></div>\
                </div>');
            }
            html.push('</div>');
            return html.join('');
        }

        function createMonthHTML(d)
        {
            var days = [];
            d.setDate(1);
            var lastMonth = new Date(d.getTime() - 86400000);
            var now = new Date();

            var dayOfWeek = d.getDay();
            if((dayOfWeek == 0) && (opt.startOfWeek == 'monday')) {
                // add one week
                dayOfWeek = 7;
            }

            if (dayOfWeek > 0)
            {
                for (var i = dayOfWeek; i > 0; i--)
                {
                    var day = new Date(d.getTime() - 86400000*i);
                    var valid = true;
                    if (opt.startDate && compare_day(day,opt.startDate) < 0) valid = false;
                    if (opt.endDate && compare_day(day,opt.endDate) > 0) valid = false;
                    days.push({type:'lastMonth',day: day.getDate(),time:day.getTime(), valid:valid });
                }
            }
            var toMonth = d.getMonth();
            for(var i=0; i<40; i++)
            {
                var today = moment(d).add('days', i).toDate();
                var valid = true;
                if (opt.startDate && compare_day(today,opt.startDate) < 0) valid = false;
                if (opt.endDate && compare_day(today,opt.endDate) > 0) valid = false;
                days.push({type: today.getMonth() == toMonth ? 'toMonth' : 'nextMonth',day: today.getDate(),time:today.getTime(), valid:valid });
            }
            var html = [];
            for(var week=0; week<6; week++)
            {
                if (days[week*7].type == 'nextMonth') break;
                html.push('<tr>');
                for(var day = 0; day<7; day++)
                {
                    var _day = (opt.startOfWeek == 'monday') ? day+1 : day;
                    var today = days[week*7+_day];
                    var highlightToday = moment(today.time).format('L') == moment(now).format('L');
                    today.extraClass = '';
                    today.tooltip = '';
                    if(opt.beforeShowDay && typeof opt.beforeShowDay == 'function')
                    {
                        var _r = opt.beforeShowDay(moment(today.time).toDate());
                        today.valid = _r[0];
                        today.extraClass = _r[1] || '';
                        today.tooltip = _r[2] || '';
                        if (today.tooltip != '') today.extraClass += ' has-tooltip ';
                    }
                    html.push('<td><div time="'+today.time+'" title="'+today.tooltip+'" class="day '+today.type+' '+today.extraClass+' '+(today.valid ? 'valid' : 'invalid')+' '+(highlightToday?'real-today':'')+'">'+today.day+'</div></td>');
                }
                html.push('</tr>');
            }
            return html.join('');
        }

        function getLanguages()
        {
            if (opt.language == 'auto')
            {
                var language = navigator.language ? navigator.language : navigator.browserLanguage;
                if (!language) return $.dateRangePickerLanguages['en'];
                var language = language.toLowerCase();
                for(var key in $.dateRangePickerLanguages)
                {
                    if (language.indexOf(key) != -1)
                    {
                        return $.dateRangePickerLanguages[key];
                    }
                }
                return $.dateRangePickerLanguages['en'];
            }
            else if ( opt.language && opt.language in $.dateRangePickerLanguages)
            {
                return $.dateRangePickerLanguages[opt.language];
            }
            else
            {
                return $.dateRangePickerLanguages['en'];
            }
        }

        function lang(t)
        {
            return (t in langs)? langs[t] : t;
        }


    };
})(jQuery);
