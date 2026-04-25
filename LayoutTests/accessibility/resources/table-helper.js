function tableProperties(table) {
    let result = "";
    result += table.attributesOfColumnHeaders() + "\n";
    result += table.attributesOfRowHeaders() + "\n";
    result += table.attributesOfColumns() + "\n";
    result += table.attributesOfRows() + "\n";
    result += table.attributesOfVisibleCells() + "\n";
    result += table.attributesOfHeader() + "\n";
    return result;
}
