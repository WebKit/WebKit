import boto3
import json
import urllib.parse  # pylint: disable=E0611

print('Loading function')
dynamodb_client = boto3.client('dynamodb')
table_name = 'minified-archives.webkit.org'
s3 = boto3.client('s3')


def lambda_handler(event, context):
    print("Received event: " + json.dumps(event, indent=2))

    key = urllib.parse.unquote_plus(event['Records'][0]['s3']['object']['key'], encoding='utf-8')  # pylint: disable=E1101
    # ex: mac-sierra-x86_64-debug/218331.zip

    split_key = key.split('/')
    identifier = split_key[0]           # mac-sierra-x86_64-debug
    filename = split_key[1]             # 218331.zip
    revision = filename.split('.')[0]   # 218331

    print ("identifier: " + identifier)
    print ("revision: " + revision)

    try:
        item = {'identifier': {'S': identifier}, 'revision': {'N': revision}}
        response = dynamodb_client.delete_item(TableName=table_name, Key=item)
        return response
    except Exception as e:
        print(e)
        print('Error deleting item: {}\nfrom database: {}.'.format(item, table_name))
        raise e
