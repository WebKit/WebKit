with open('experiment.txt', 'r') as file:
    lines = file.readlines()
    curr_filename = ''
    keyframe = 0
    actual_value = 0
    estimate_value = 0
    print('filename, estimated value (b), actual value (b)')
    for line in lines:
        if line.startswith('input:'):
            curr_filename = line[13:].strip()
        if line.startswith('estimated'):
            estimate_value = float(line[19:].strip())
        if line.startswith('frame:'):
            actual_value += float(line[line.find('size')+6:line.find('total')-2])
        if line.startswith('****'):
            print(f'{curr_filename}, {estimate_value}, {actual_value}')
            estimate_value = 0
            actual_value = 0
