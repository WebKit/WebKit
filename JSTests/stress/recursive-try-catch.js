//@ defaultNoSamplingProfilerRun
// This test should run to completion without excessive memory usage

let maxHeapAllowed = 10 * 1024 * 1024; // This test should run using much less than 10MB.
let badFunction = undefined;
let loggedError = undefined;

function logError(error)
{
    loggedError = error;
}

function tryCallingBadFunction()
{
    try {
        badFunction(42);
    } catch(error) {
        logError(error);
    }

    recurse();
}

function recurse()
{
    // Make the frame big to run out of stack with fewer recursive calls.
    let val1, val2, val3, val4, val5, val6, val7, val8, val9, val10;
    let val11, val12, val13, val14, val15, val16, val17, val18, val19, val20;
    let val21, val22, val23, val24, val25, val26, val27, val28, val29, val30;
    let val31, val32, val33, val34, val35, val36, val37, val38, val39, val40;
    let val41, val42, val43, val44, val45, val46, val47, val48, val49, val50;
    let val51, val52, val53, val54, val55, val56, val57, val58, val59, val60;
    let val61, val62, val63, val64, val65, val66, val67, val68, val69, val70;
    let val71, val72, val73, val74, val75, val76, val77, val78, val79, val80;
    let val81, val82, val83, val84, val85, val86, val87, val88, val89, val90;
    let val91, val92, val93, val94, val95, val96, val97, val98, val99, val100;
    let val101, val102, val103, val104, val105, val106, val107, val108, val109, val110;
    let val111, val112, val113, val114, val115, val116, val117, val118, val119, val120;
    let val121, val122, val123, val124, val125, val126, val127, val128, val129, val130;
    let val131, val132, val133, val134, val135, val136, val137, val138, val139, val140;
    let val141, val142, val143, val144, val145, val146, val147, val148, val149, val150;
    let val151, val152, val153, val154, val155, val156, val157, val158, val159, val160;
    let val161, val162, val163, val164, val165, val166, val167, val168, val169, val170;
    let val171, val172, val173, val174, val175, val176, val177, val178, val179, val180;
    let val181, val182, val183, val184, val185, val186, val187, val188, val189, val190;
    let val191, val192, val193, val194, val195, val196, val197, val198, val199, val200;
    let val201, val202, val203, val204, val205, val206, val207, val208, val209, val210;
    let val211, val212, val213, val214, val215, val216, val217, val218, val219, val220;
    let val221, val222, val223, val224, val225, val226, val227, val228, val229, val230;
    let val231, val232, val233, val234, val235, val236, val237, val238, val239, val240;
    let val241, val242, val243, val244, val245, val246, val247, val248, val249, val250;
    let val251, val252, val253, val254, val255, val256, val257, val258, val259, val260;
    let val261, val262, val263, val264, val265, val266, val267, val268, val269, val270;
    let val271, val272, val273, val274, val275, val276, val277, val278, val279, val280;
    let val281, val282, val283, val284, val285, val286, val287, val288, val289, val290;
    let val291, val292, val293, val294, val295, val296, val297, val298, val299, val300;
    let val301, val302, val303, val304, val305, val306, val307, val308, val309, val310;
    let val311, val312, val313, val314, val315, val316, val317, val318, val319, val320;
    let val321, val322, val323, val324, val325, val326, val327, val328, val329, val330;
    let val331, val332, val333, val334, val335, val336, val337, val338, val339, val340;
    let val341, val342, val343, val344, val345, val346, val347, val348, val349, val350;
    let val351, val352, val353, val354, val355, val356, val357, val358, val359, val360;
    let val361, val362, val363, val364, val365, val366, val367, val368, val369, val370;
    let val371, val372, val373, val374, val375, val376, val377, val378, val379, val380;
    let val381, val382, val383, val384, val385, val386, val387, val388, val389, val390;
    let val391, val392, val393, val394, val395, val396, val397, val398, val399, val400;

    tryCallingBadFunction();
}

function test()
{
    try {
        recurse();
    } catch(error) {
        if (error != "RangeError: Maximum call stack size exceeded.")
            throw "Expected: \"RangeError: Maximum call stack size exceeded.\", but got: " + error;

        let heapUsed = gcHeapSize();
        if (heapUsed > maxHeapAllowed)
            throw "Used too much heap.  Limit was " + maxHeapAllowed + " bytes, but we used " + heapUsed + " bytes.";
    }

    if (loggedError.name != "TypeError")
        throw "Expected logged error to be: \"TypeError\", but got: " + loggedError.name;
}

test();

