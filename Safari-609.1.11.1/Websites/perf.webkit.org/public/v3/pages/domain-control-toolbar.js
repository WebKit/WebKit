
class DomainControlToolbar extends Toolbar {
    constructor(name, numberOfDays)
    {
        super(name);
        this._startTime = null;
        this._numberOfDays = numberOfDays;
        this._setByUser = false;
        this._callback = null;
        this._present = Date.now();
        this._millisecondsPerDay = 24 * 3600 * 1000;
    }

    startTime() { return this._startTime || (this._present - this._numberOfDays * this._millisecondsPerDay); }
    endTime() { return this._present; }
    setByUser() { return this._setByUser; }

    setStartTime(startTime)
    {
        this.setNumberOfDays(Math.max(1, Math.round((this._present - startTime) / this._millisecondsPerDay)));
        this._startTime = startTime;
    }

    numberOfDays()
    {
        return this._numberOfDays;
    }

    setNumberOfDays(numberOfDays, setByUser)
    {
        if (!numberOfDays)
            return;

        this._startTime = null;
        this._numberOfDays = numberOfDays;
        this._setByUser = !!setByUser;
    }

}
