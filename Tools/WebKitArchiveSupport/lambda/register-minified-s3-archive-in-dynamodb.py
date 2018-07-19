import boto3
import json
import time
import urllib.parse  # pylint: disable=E0611

DAYS_TO_KEEP = 910
EPOCH_DAY = 86400
ARCHIVE_TABLE_NAME = 'minified-archives.webkit.org'
PLATFORM_TABLE_NAME = 'minified-platforms.webkit.org'
URL_PREFIX = 'https://s3-us-west-2.amazonaws.com'

dynamodb_client = boto3.client('dynamodb')
s3 = boto3.client('s3')


def lambda_handler(event, context):
    print("Received event: " + json.dumps(event, indent=2))

    # Get the bucket name from the event
    bucket = event['Records'][0]['s3']['bucket']['name']
    key = urllib.parse.unquote_plus(event['Records'][0]['s3']['object']['key'], encoding='utf-8')  # pylint: disable=E1101
    # ex: mac-sierra-x86_64-debug/218331.zip

    split_key = key.split('/')
    identifier = split_key[0]           # mac-sierra-x86_64-debug
    filename = split_key[1]             # 218331.zip
    revision = filename.split('.')[0]   # 218331
    creation_time = int(time.time())
    expiration_time = creation_time + DAYS_TO_KEEP * EPOCH_DAY

    creation_time = str(creation_time)
    expiration_time = str(expiration_time)
    s3_url = '/'.join([URL_PREFIX, bucket, key])

    try:
        item = {
            'identifier': {'S': identifier},
            'revision': {'N': revision},
            's3_url': {'S': s3_url},
            'creationTime': {'N': creation_time},
            'expirationTime': {'N': expiration_time},
        }
        print('Item: {}'.format(item))
        dynamodb_client.put_item(TableName=ARCHIVE_TABLE_NAME, Item=item)

        platform = {
            'identifier': {'S': identifier},
            'creationTime': {'N': creation_time},
            'expirationTime': {'N': expiration_time},
        }
        print('Item: {}'.format(platform))
        dynamodb_client.put_item(TableName=PLATFORM_TABLE_NAME, Item=platform)

        return s3_url
    except Exception as e:
        print(e)
        print('Error registering item: {}\nfrom bucket {}.'.format(item, bucket))
        raise e
